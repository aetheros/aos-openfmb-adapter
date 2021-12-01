// vim: sw=4 expandtab
// Copyright (c) Aetheros, Inc.  See COPYRIGHT
#include <aos/AppMain.hpp>
#include <aos/Log.hpp>
#include <aos/Mqtt.hpp>

#include <m2m/AppEntity.hpp>
#include <m2m/Timestamp.hpp>

#include <xsd/m2m/Subscription.hpp>
#include <xsd/m2m/ContentInstance.hpp>
#include <xsd/m2m/Names.hpp>

#include <xsd/mtrsvc/MeterReadSchedulePolicy.hpp>
#include <xsd/m2m/Names.hpp>

#include <commonmodule/commonmodule.pb.h>
#include <metermodule/metermodule.pb.h>
#include <loadmodule/loadmodule.pb.h>

#include "protobuf_xsd.hpp"

#include <uuid/uuid.h>

using std::chrono::seconds;
using std::chrono::minutes;
using std::chrono::duration_cast;

constexpr int MqttPublishQos = 0;

uuid_t meterUuid;

std::string toString(const uuid_t uuid)
{
    char buf[40];

    uuid_unparse(uuid, buf);

    return std::string(buf);
}

m2m::AppEntity appEntity;

aos::mqtt::Client mqttClient("aos-openfmb-adapter");

// Receive OneM2M notifications here 
void notificationCallback(m2m::Notification notification);

// Create a OneM2M subscription to the metering service API results
bool create_subscription();

// Create a meter read policy object to configure the metering service API to begin reading for us
bool create_meter_read_policy();

[[noreturn]] void usage(const char *prog)
{
        fprintf(stderr, "Usage %s [-d] [-p <POA-URI>|-a <IPADDR[:PORT]>] [CSE-URI]\n", prog);
        exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    // scope the application main loop
    aos::AppMain appMain;

    char hardcodedUuid[] = "f72ba8f6-d390-11eb-ab5f-bf67369bdb6e";

    uuid_parse(hardcodedUuid, meterUuid);

    const char *poaUri = nullptr;
    const char *poaAddr = nullptr;

    std::string brokerAddress = "127.0.0.1";
    unsigned short brokerPort = 1883;

    int opt;
    while ((opt = getopt(argc, argv, "dp:a:P:A:")) != -1)
    {
        switch (opt)
        {
        case 'd':
            aos::setLogLevel(aos::LogLevel::LOG_DEBUG);
            break;
        case 'p':
            poaUri = optarg;
            break;
        case 'a':
            poaAddr = optarg;
            break;
        case 'P':
            brokerPort = std::stoi(optarg);
            break;
        case 'A':
            brokerAddress = optarg;
            break;
        default:
            usage(argv[0]);
            break;
        }
    }

    if (argc-optind > 1)
    {
        usage(argv[0]);
    }

    // initialize the AE object
    appEntity = m2m::AppEntity(notificationCallback);

    if (argc-optind == 1)
    {
        appEntity.setCseUri(argv[optind]);
    }

    if (poaUri)
    {
        appEntity.setPoaUri(poaUri);
    }
    else if (poaAddr)
    {
        appEntity.setPoaAddr(poaAddr);
    }

#   if DEMO_TLS
    mqttClient.setTlsConfig("/home/apps/demo_ca.pem",
				"/home/apps/device.pem",
				"/home/apps/device.key",
				"",
				"",
				aos::mqtt::TLSVerifyMethod::Peer);
#   endif

    if (!mqttClient.connect(brokerAddress, brokerPort, 60, true))
    {
        logError("Failed to connect to MQTT broker");
        return EXIT_FAILURE;
    }

    // The mqtt client must do work somewhere, delegate it to a background thread
    aos::mqtt::ThreadedBackgroundWorker worker(mqttClient);

    while (true)
    {
        seconds backoffSeconds{30};

        // activate the AE (checks registration, registers if necessary, updates poa if necessary)
        logInfo("activating");
        while (!appEntity.activate())
        {
            auto failureReason = appEntity.getActivationFailureReason();
            logError("activation failed: reason: " << failureReason);
            if (failureReason != m2m::ActivationFailureReason::Timeout &&
                failureReason != m2m::ActivationFailureReason::NotRegistered)
            {
                logInfo("retrying in " << backoffSeconds.count() << " seconds");
                std::this_thread::sleep_for(backoffSeconds);
                if (backoffSeconds < minutes{16})
                    backoffSeconds *= 2;
            }
        }

        logInfo("activated");
        if (!create_subscription())
        {
            logError("subscription creation failed");
            std::this_thread::sleep_for(seconds{30});
            continue;
        }

        if (!create_meter_read_policy())
        {
            logError("meter read policy creation failed");
            std::this_thread::sleep_for(seconds{30});
            continue;
        }

        break;
    }

    appEntity.waitForever();
}

bool create_subscription()
{
    logDebug("-");

    xsd::m2m::Subscription subscription = xsd::m2m::Subscription::Create();

    subscription.creator = std::string();

    // set the subscription's name
    // The subscription name should be consistent and unique to your app/instance
    subscription.resourceName = "metersv-openfmb-sub-01";

    // have all resource attributes be provided in the notifications
    subscription.notificationContentType = xsd::m2m::NotificationContentType::all_attributes;

    // set the notification destination to be this AE.
    subscription.notificationURI = xsd::m2m::ListOfURIs();
    subscription.notificationURI->push_back(appEntity.getResourceId());

    // set eventNotificationCriteria to creation and deletion of child resources
    xsd::m2m::EventNotificationCriteria eventNotificationCriteria;
    eventNotificationCriteria.notificationEventType.assign().push_back(xsd::m2m::NotificationEventType::Create_of_Direct_Child_Resource);
    subscription.eventNotificationCriteria = std::move(eventNotificationCriteria);

    m2m::Request request = appEntity.newRequest(xsd::m2m::Operation::Create, m2m::To{"./metersvc/reads"});
    request.req->resultContent = xsd::m2m::ResultContent::Nothing;
    request.req->resourceType = xsd::m2m::ResourceType::subscription;
    request.req->primitiveContent = xsd::toAnyNamed(subscription);

    appEntity.sendRequest(request);
    auto response = appEntity.getResponse(request);

    logInfo("subscription: " << toString(response->responseStatusCode));

    return (response->responseStatusCode == xsd::m2m::ResponseStatusCode::CREATED || response->responseStatusCode == xsd::m2m::ResponseStatusCode::CONFLICT);
}

bool create_meter_read_policy()
{
    logDebug("-");

    xsd::mtrsvc::ScheduleInterval scheduleInterval;
    scheduleInterval.end = nullptr;
    scheduleInterval.start = "2020-06-19T00:00:00";

    xsd::mtrsvc::TimeSchedule timeSchedule;
    timeSchedule.recurrencePeriod = 1; // Request reads every 1 second
    timeSchedule.scheduleInterval = std::move(scheduleInterval);

    xsd::mtrsvc::MeterReadSchedule meterReadSchedule;
    meterReadSchedule.readingType = "powerQuality";
    meterReadSchedule.timeSchedule = std::move(timeSchedule);

    xsd::mtrsvc::MeterServicePolicy meterServicePolicy;
    meterServicePolicy.meterReadSchedule = std::move(meterReadSchedule);

    xsd::m2m::ContentInstance policyInst = xsd::m2m::ContentInstance::Create();
    policyInst.content = xsd::toAnyTypeUnnamed(meterServicePolicy);

    policyInst.resourceName = "metersvc-openfmb-pol-01";

    m2m::Request request = appEntity.newRequest(xsd::m2m::Operation::Create, m2m::To{"./metersvc/policies"});
    request.req->resultContent = xsd::m2m::ResultContent::Nothing;
    request.req->resourceType = xsd::m2m::ResourceType::contentInstance;
    request.req->primitiveContent = xsd::toAnyNamed(policyInst);

    appEntity.sendRequest(request);
    auto response = appEntity.getResponse(request);

    logInfo("policy creation: " << toString(response->responseStatusCode));

    return (response->responseStatusCode == xsd::m2m::ResponseStatusCode::CREATED || response->responseStatusCode == xsd::m2m::ResponseStatusCode::CONFLICT);
}

bool translate(commonmodule::ReadingMMXU * mmxu, const xsd::mtrsvc::MeterSvcData & meterSvcData, const std::chrono::seconds & timestamp) {
    const auto& pq = meterSvcData.powerQuality;

    set(mmxu->mutable_hz(), pq->frequency, timestamp);

    auto demand = mmxu->mutable_w();
    set(demand->mutable_phsa(), pq->activePowerA, nullptr, timestamp);
    set(demand->mutable_phsb(), pq->activePowerB, nullptr, timestamp);
    set(demand->mutable_phsc(), pq->activePowerC, nullptr, timestamp);

    auto va = mmxu->mutable_va();
    set(va->mutable_phsa(), pq->apparentPowerA, nullptr, timestamp);
    set(va->mutable_phsb(), pq->apparentPowerB, nullptr, timestamp);
    set(va->mutable_phsc(), pq->apparentPowerC, nullptr, timestamp);

    auto var = mmxu->mutable_var();
    set(var->mutable_phsa(), pq->reactivePowerA, nullptr, timestamp);
    set(var->mutable_phsb(), pq->reactivePowerB, nullptr, timestamp);
    set(var->mutable_phsc(), pq->reactivePowerC, nullptr, timestamp);

    auto voltage = mmxu->mutable_phv();
    set(voltage->mutable_phsa(), pq->voltageA, pq->voltageAngleA, timestamp);
    set(voltage->mutable_phsb(), pq->voltageB, pq->voltageAngleB, timestamp);
    set(voltage->mutable_phsc(), pq->voltageC, pq->voltageAngleC, timestamp);

    auto current = mmxu->mutable_a();
    set(current->mutable_phsa(), pq->currentA, nullptr, timestamp);
    set(current->mutable_phsb(), pq->currentB, nullptr, timestamp);
    set(current->mutable_phsc(), pq->currentC, nullptr, timestamp);
    
    return true;
}

void notificationCallback(m2m::Notification notification)
{
    uuid_t messageUuid;

    logDebug("-");
    if (!notification.notificationEvent.isSet())
    {
        logWarn("notification has no notificationEvent");
        return;
    }

    logDebug("got notification type " << toString(notification.notificationEvent->notificationEventType));

    if (notification.notificationEvent->notificationEventType != xsd::m2m::NotificationEventType::Create_of_Direct_Child_Resource)
    {
        return;
    }

    // Retrieve meter reported data
    auto contentInstance = notification.notificationEvent->representation->extractNamed<xsd::m2m::ContentInstance>();
    auto meterRead = contentInstance.content->extractUnnamed<xsd::mtrsvc::MeterRead>();
    auto &meterSvcData = *meterRead.meterSvcData;

    if (meterSvcData.powerQuality.isSet())
    {
        // Default to 0, which is treated as null elsewhere
        std::chrono::seconds timestamp(0);

        // Retrieve timestamp from report if available
        if (meterSvcData.readTimeLocal.isSet())
        {
            timestamp = m2m::toEpochSeconds(*meterSvcData.readTimeLocal);
        }

        metermodule::MeterReadingProfile profile;

        auto reading = profile.mutable_meterreading();
        auto meter = profile.mutable_meter();
        auto conductingEquipment = meter->mutable_conductingequipment();

        auto readingMessageInfo = profile.mutable_readingmessageinfo();
        auto messageInfo = readingMessageInfo->mutable_messageinfo();
        setTimestamp(messageInfo, timestamp);

        std::string meterId = "Some Meter";
        if (meterSvcData.meterId.isSet())
        {
            meterId = *meterSvcData.meterId;
        }
        logInfo("meterId = " << meterId);

        *conductingEquipment->mutable_mrid() = toString(meterUuid);
        initialize(conductingEquipment->mutable_namedobject(),
                   meterId, // name
                   "Meter" // description
                   );

        auto mmxu = reading->mutable_readingmmxu();
        translate(mmxu, meterSvcData, timestamp);

        uuid_generate_time_safe(messageUuid);

        auto messageIdentity = messageInfo->mutable_identifiedobject();
        messageIdentity->mutable_mrid()->set_value(toString(messageUuid));

        std::string openfmbPayload;
        profile.SerializeToString(&openfmbPayload);

        logDebug("publishing");
        mqttClient.publish("openfmb/metermodule/MeterReadingProfile/" + toString(meterUuid), MqttPublishQos, openfmbPayload, false);
    }

    if (meterSvcData.powerQuality.isSet())
    {
        logInfo("powerQuality: " << *meterSvcData.powerQuality);
    }

    if (meterSvcData.summations.isSet())
    {
        logInfo("summations: " << *meterSvcData.summations);
    }
}

