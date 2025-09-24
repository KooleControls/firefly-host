#include "TimeSpan.h"

// Private constructor
TimeSpan::TimeSpan(long seconds) : seconds(seconds) {}

// Arithmetic operators
TimeSpan TimeSpan::operator+(const TimeSpan& other) const {
    return TimeSpan(seconds + other.seconds);
}

TimeSpan TimeSpan::operator-(const TimeSpan& other) const {
    return TimeSpan(seconds - other.seconds);
}

TimeSpan& TimeSpan::operator+=(const TimeSpan& other) {
    seconds += other.seconds;
    return *this;
}

TimeSpan& TimeSpan::operator-=(const TimeSpan& other) {
    seconds -= other.seconds;
    return *this;
}

// Comparison operators
bool TimeSpan::operator==(const TimeSpan& other) const { 
    return seconds == other.seconds; 
}

bool TimeSpan::operator!=(const TimeSpan& other) const { 
    return seconds != other.seconds; 
}

bool TimeSpan::operator<(const TimeSpan& other) const { 
    return seconds < other.seconds; 
}

bool TimeSpan::operator<=(const TimeSpan& other) const { 
    return seconds <= other.seconds; 
}

bool TimeSpan::operator>(const TimeSpan& other) const { 
    return seconds > other.seconds; 
}

bool TimeSpan::operator>=(const TimeSpan& other) const { 
    return seconds >= other.seconds; 
}

// Component functions with adjustment for negative values
int TimeSpan::Seconds() const {
    int s = seconds % 60;
    if (s < 0)
        s += 60;
    return s;
}

long TimeSpan::TotalSeconds() const { 
    return seconds; 
}

int TimeSpan::Minutes() const {
    long totalMinutes = seconds / 60;
    int m = totalMinutes % 60;
    if (m < 0)
        m += 60;
    return m;
}

long TimeSpan::TotalMinutes() const { 
    return seconds / 60; 
}

int TimeSpan::Hours() const {
    long totalHours = seconds / 3600;
    int h = totalHours % 24;
    if (h < 0)
        h += 24;
    return h;
}

long TimeSpan::TotalHours() const { 
    return seconds / 3600; 
}

// Factory methods
TimeSpan TimeSpan::FromHours(long hours) {
    return TimeSpan(hours * 3600);
}

TimeSpan TimeSpan::FromMinutes(long minutes) {
    return TimeSpan(minutes * 60);
}

TimeSpan TimeSpan::FromSeconds(long secs) {
    return TimeSpan(secs);
}

TimeSpan TimeSpan::Zero()
{
    return TimeSpan(0);
}
