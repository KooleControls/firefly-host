#pragma once

class TimeSpan {
    long seconds;
    explicit TimeSpan(long seconds);

public:
    // Arithmetic operators
    TimeSpan operator+(const TimeSpan& other) const;
    TimeSpan operator-(const TimeSpan& other) const;
    TimeSpan& operator+=(const TimeSpan& other);
    TimeSpan& operator-=(const TimeSpan& other);

    // Comparison operators
    bool operator==(const TimeSpan& other) const;
    bool operator!=(const TimeSpan& other) const;
    bool operator<(const TimeSpan& other) const;
    bool operator<=(const TimeSpan& other) const;
    bool operator>(const TimeSpan& other) const;
    bool operator>=(const TimeSpan& other) const;

    /// @brief Seconds component modulo 60 (0-59)
    int Seconds() const;
    long TotalSeconds() const;

    /// @brief Minutes component modulo 60 (0-59)
    int Minutes() const;
    long TotalMinutes() const;

    /// @brief Hours component modulo 24 (0-23)
    int Hours() const;
    long TotalHours() const;

    // Factory methods
    static TimeSpan FromHours(long hours);
    static TimeSpan FromMinutes(long minutes);
    static TimeSpan FromSeconds(long secs);

    static TimeSpan Zero();
};

