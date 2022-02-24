#pragma once

#include <condition_variable>
#include <mutex>
#include <string>

#include <m2m/AppEntity.hpp>
#include <xsd/m2m/CommonTypes.hpp>
#include <xsd/m2m/ContentInstance.hpp>
#include <xsd/m2m/CSEBase.hpp>
#include <aos/Log.hpp>
#include <xsd/m2m/Names.hpp>
#include <xsd/m2m/Subscription.hpp>

template <typename TXsd>
bool tryExtract(const xsd::xs::Any & any, std::string & parentId, std::string & resourceName)
{
	try
	{
		auto resource = any.extractNamed<TXsd>();
		logDebug("resource.resourceName = \"" << *resource.resourceName << "\"");

		if (resource.resourceName.isSet())
		{
			resourceName = *resource.resourceName;
		}

		if (resource.parentID.isSet())
		{
			logDebug("resource.parentID = \"" << *resource.parentID);
			parentId = *resource.parentID;
		}
		else
		{
			logDebug("resource.parentID null");
			parentId = "";
		}

		return true;
	}
	catch (std::exception & e)
	{
		logError(e.what());
		return false;
	}
}

template <>
bool tryExtract<xsd::m2m::CSEBase>(const xsd::xs::Any & any, std::string & parentId, std::string & resourceName)
{
	try
	{
		auto resource = any.extractNamed<xsd::m2m::CSEBase>();
		logDebug("resource.resourceName = \"" << *resource.resourceName << "\"");

		if (resource.CSE_ID.isSet())
		{
			resourceName = *resource.CSE_ID;
		}

		parentId = "";

		return true;
	}
	catch (std::exception & e)
	{
		logError(e.what());
		return false;
	}
}

std::string getAbsoluteUri(m2m::AppEntity & appEntity, const std::string & identifier)
{
	std::vector<std::string> pathElements;

	std::string resourceId = identifier;

	while (!resourceId.empty())
	{
		m2m::Request request = appEntity.newRequest(xsd::m2m::Operation::Retrieve, m2m::To{resourceId});
		appEntity.sendRequest(request);
		auto response = appEntity.getResponse(request);

		logDebug("retrieve \"" << resourceId << "\": " << toString(response->responseStatusCode));
		if (response->responseStatusCode != xsd::m2m::ResponseStatusCode::OK)
		{
			logError("Failed to retrieve config from \"" << resourceId << "\" with error code " << toString(response->responseStatusCode));
			return "";
		}

		if (!response->primitiveContent.isSet())
		{
			return "";
		}

		auto primitiveContent = response->primitiveContent->getAny();

		std::string parentResourceId, resourceName;
		if (tryExtract<xsd::m2m::Container>(primitiveContent, parentResourceId, resourceName))
		{
			pathElements.insert(pathElements.begin(), resourceName);
			resourceId = parentResourceId;
		}
		else if (tryExtract<xsd::m2m::ContentInstance>(primitiveContent, parentResourceId, resourceName))
		{
			pathElements.insert(pathElements.begin(), resourceName);
			resourceId = parentResourceId;
		}
		else if (tryExtract<xsd::m2m::Subscription>(primitiveContent, parentResourceId, resourceName))
		{
			pathElements.insert(pathElements.begin(), resourceName);
			resourceId = parentResourceId;
		}
		else if (tryExtract<xsd::m2m::CSEBase>(primitiveContent, parentResourceId, resourceName))
		{
			pathElements.insert(pathElements.begin(), resourceName);
			resourceId = parentResourceId;
		}
	}

	std::ostringstream out;
	if (!pathElements.empty())
	{
		out << pathElements[0];
	}

	for (size_t i = 1; i < pathElements.size(); i++)
	{
		out << '/' << pathElements[i];
	}

	return out.str();
}

class Configuration
{
public:
    Configuration(m2m::AppEntity * appEntity,
                  const std::string & configContainerName,
                  const std::string & configContentInstanceName = "config",
	              const std::string & configContainerSubscriptionName = "config-sub"
            ):
        appEntity(appEntity),
	    configContainerName(configContainerName),
	    configContentInstanceName(configContentInstanceName),
	    configContainerSubscriptionName(configContainerSubscriptionName),
	    _configurationChanged(false)
    {}

    boost::optional<xsd::m2m::ContentInstance> getConfiguration()
    {
		if (!configuration)
		{
			try
			{
				retrieveConfiguration();
			}
			catch (std::exception & e)
			{
				logError(e.what());
			}
		}

		{
			std::unique_lock<std::mutex> lock(mutex);
			_configurationChanged = false;
		}

        return configuration;
    }

    xsd::m2m::ContentInstance waitForConfiguration()
    {
		getConfiguration();

		std::unique_lock<std::mutex> lock(mutex);

		while (!configuration)
		{
			condition.wait(lock);
		}

		return *configuration;
    }

    void processNotification(m2m::Notification & notification)
    {
        if (!notification.notificationEvent.isSet())
        {
            return;
        }

        if (notification.notificationEvent->notificationEventType != xsd::m2m::NotificationEventType::Create_of_Direct_Child_Resource)
        {
            return;
        }

		if (!notification.subscriptionReference.isSet())
		{
			return;
		}

		if (*notification.subscriptionReference != configSubscriptionUri)
		{
			return;
		}

		try
		{
			configuration = notification.notificationEvent->representation->extractNamed<xsd::m2m::ContentInstance>();

			{
				std::unique_lock<std::mutex> lock(mutex);
				logDebug("Config changed");
				_configurationChanged = true;
			}

			condition.notify_all();
		}
		catch (std::exception & e)
		{
			logError(e.what());
		}
    }

    bool createConfigContainer()
    {
		logDebug("-");

		auto container = xsd::m2m::Container::Create();

		container.creator = std::string();

		// set the subscription's name
		// The subscription name should be consistent and unique to your app/instance
		container.resourceName = configContainerName;
		logDebug("configContainerName = " << configContainerName);

		// set eventNotificationCriteria to creation and deletion of child resources
		m2m::Request request = appEntity->newRequest(xsd::m2m::Operation::Create, m2m::To{"."});
		request.req->resultContent = xsd::m2m::ResultContent::Attributes;
		request.req->resourceType = xsd::m2m::ResourceType::container;
		request.req->primitiveContent = xsd::toAnyNamed(container);

		appEntity->sendRequest(request);
		auto response = appEntity->getResponse(request);

		logInfo("container creation: " << toString(response->responseStatusCode));

		return (response->responseStatusCode == xsd::m2m::ResponseStatusCode::CREATED || response->responseStatusCode == xsd::m2m::ResponseStatusCode::CONFLICT);
    }

    bool createConfigSubscription()
    {
		xsd::m2m::Subscription subscription = xsd::m2m::Subscription::Create();

		subscription.creator = std::string();

		// set the subscription's name
		subscription.resourceName = configContainerSubscriptionName;

		// have all resource attributes be provided in the notifications
		subscription.notificationContentType = xsd::m2m::NotificationContentType::all_attributes;

		// set the notification destination to be this AE.
		subscription.notificationURI = xsd::m2m::ListOfURIs();
		subscription.notificationURI->push_back(appEntity->getResourceId());

		// set eventNotificationCriteria to creation and deletion of child resources
		xsd::m2m::EventNotificationCriteria eventNotificationCriteria;
		eventNotificationCriteria.notificationEventType.assign().push_back(xsd::m2m::NotificationEventType::Create_of_Direct_Child_Resource);
		subscription.eventNotificationCriteria = std::move(eventNotificationCriteria);

		m2m::Request request = appEntity->newRequest(xsd::m2m::Operation::Create, m2m::To{"./" + configContainerName});
		request.req->resultContent = xsd::m2m::ResultContent::Nothing;
		request.req->resourceType = xsd::m2m::ResourceType::subscription;
		request.req->primitiveContent = xsd::toAnyNamed(subscription);

		appEntity->sendRequest(request);
		auto response = appEntity->getResponse(request);

		logInfo("subscription: " << toString(response->responseStatusCode));

		if (response->responseStatusCode != xsd::m2m::ResponseStatusCode::CREATED && response->responseStatusCode != xsd::m2m::ResponseStatusCode::CONFLICT)
		{
			return false;
		}

		std::string subscriptionFullPath = "./" + configContainerName + "/" + configContainerSubscriptionName;

		configSubscriptionUri = getAbsoluteUri(*appEntity, subscriptionFullPath);

		if (configSubscriptionUri.empty())
		{
			return false;
		}

		return true;
	}

	void setAppEntity(m2m::AppEntity * appEntity)
	{
		this->appEntity = appEntity;
	}

	bool setup()
	{
		if (!createConfigContainer())
		{
			return false;
		}

		if (!createConfigSubscription())
		{
			return false;
		}

		return true;
	}

	bool configurationChanged()
	{
		return _configurationChanged;
	}

protected:

	void retrieveConfiguration()
	{
		std::string configFullPath = "./" + configContainerName + "/" + configContentInstanceName;
		logDebug("Attempting to retrieve configuration from " << configFullPath);
		m2m::Request request = appEntity->newRequest(xsd::m2m::Operation::Retrieve, m2m::To{configFullPath});

		appEntity->sendRequest(request); // "[json.exception.type_error.302] type must be string, but is boolean"

		auto response = appEntity->getResponse(request);

		logInfo("retrieve config: " << toString(response->responseStatusCode));
		if (response->responseStatusCode != xsd::m2m::ResponseStatusCode::OK)
		{
			logError("Failed to retrieve config from \"" << configFullPath << "\" with error code " << toString(response->responseStatusCode));
			return;
		}

		logDebug("primitive content = " << (int)response->primitiveContent.isSet());
		logDebug("config = " << response->primitiveContent->getAny().dumpJson());

		configuration = response->primitiveContent->getAny().extractNamed<xsd::m2m::ContentInstance>();
	}

    m2m::AppEntity * appEntity;
    std::string configContainerName;
    std::string configContainerSubscriptionName;
    std::string configContentInstanceName;
	std::string configSubscriptionUri;
    boost::optional<xsd::m2m::ContentInstance> configuration;
	bool _configurationChanged;
    std::mutex mutex;
	std::condition_variable condition;
};
