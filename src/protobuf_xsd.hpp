#pragma once

#include <chrono>
#include <type_traits>

#include <xsd/Xs.hpp>
#include <m2m/Timestamp.hpp>
#include <commonmodule/commonmodule.pb.h>

// Convenience functions
template <typename T>
void setTimestamp(T * t, const std::chrono::seconds & timestamp)
{
    constexpr std::chrono::seconds Epoch(0);
    if (timestamp != Epoch)
	{
        t->mutable_t()->set_seconds(timestamp.count());
    }
}

template <>
void setTimestamp(commonmodule::MessageInfo * messageInfo, const std::chrono::seconds & timestamp)
{
    constexpr std::chrono::seconds Epoch(0);
    if (timestamp != Epoch)
	{
        messageInfo->mutable_messagetimestamp()->set_seconds(timestamp.count());
    }
}


template <typename TValue,
          typename std::enable_if<std::is_integral<TValue>::value, bool>::type = true >
void set(commonmodule::AnalogueValue * v, const TValue value)
{
    v->mutable_i()->set_value(value);
}

template <typename TValue,
          typename std::enable_if<std::is_floating_point<TValue>::value, bool>::type = true >
void set(commonmodule::AnalogueValue * v, const TValue value)
{
    v->mutable_f()->set_value(value);
}

template <typename TElement,
          typename TProtobufType,
          unsigned isOptional,
          unsigned isFundamental>
bool set(TProtobufType * p, const xsd::xs::Element<TElement, isOptional, isFundamental> & element)
{
    if (element.isSet())
	{
        set(p, static_cast<typename TElement::native_type>(*element));
        return true;
    }

    return false;
}

template <typename TElement,
          typename TProtobufType>
bool set(TProtobufType * p, const TElement & mag, const TElement & angle)
{
    if (!mag.isSet())
	{
        return false;
    }

    set(p->mutable_mag(), mag);
    set(p->mutable_ang(), angle);

    return true;
}

template <typename TElement,
          typename TProtobufType>
bool set(TProtobufType * p, const TElement & mag, nullptr_t)
{
    if (!mag.isSet())
	{
        return false;
    }

    set(p->mutable_mag(), mag);

    return true;
}

template <typename TValue>
void set(commonmodule::Vector * v, const TValue mag, const TValue angle)
{
    set(v->mutable_mag(), mag);
    set(v->mutable_ang(), angle);
}

template <typename TValue>
void set(commonmodule::CMV * cmv, const TValue mag, const TValue angle)
{
    set(cmv->mutable_cval(), mag, angle);
}

template <typename TValue>
void set(commonmodule::CMV * cmv, const TValue mag, nullptr_t)
{
    set(cmv->mutable_cval(), mag, nullptr);
}

template <typename TValue>
void set(commonmodule::CMV * cmv, const TValue mag, nullptr_t, const std::chrono::seconds & timestamp)
{
    set(cmv, mag, nullptr);
    setTimestamp(cmv, timestamp);
}

template <typename TValue>
void set(commonmodule::CMV * cmv, const TValue mag, const TValue angle, const std::chrono::seconds & timestamp)
{
    set(cmv, mag, angle);
    setTimestamp(cmv, timestamp);
}

template <typename TValue>
void set(commonmodule::Vector * v, const TValue mag, nullptr_t)
{
    set(v->mutable_mag(), mag);
}

template <typename TElement>
void set(commonmodule::MV * mv, const TElement value, const std::chrono::seconds & timestamp)
{
    set(mv->mutable_mag(), value);
    setTimestamp(mv, timestamp);
}

void initialize(commonmodule::IdentifiedObject * id, const std::string & description, const std::string & mrid, const std::string & name)
{
    id->mutable_description()->set_value(description);
    id->mutable_mrid()->set_value(mrid);
    id->mutable_name()->set_value(name);
}

void initialize(commonmodule::NamedObject * named, const std::string & name, const std::string & description)
{
	named->mutable_name()->set_value(name);
	named->mutable_description()->set_value(description);
}
