#pragma once

using HRTick = uint64;

class HighResolutionTimer
{
public:
	HighResolutionTimer()
	{
		m_timePoint = 0;
	}

	HRTick getTick() const
	{
		return m_timePoint;
	}

	uint64 getTickInSeconds() const
	{
		return m_timePoint / m_freq;
	}

	// return time difference in seconds, this is an utility function mainly intended for debugging/benchmarking purposes. Avoid using doubles for precise timing
	static double getTimeDiff(HRTick startTime, HRTick endTime)
	{
		return (double)(endTime - startTime) / (double)m_freq;
	}

	// returns tick difference and frequency
	static uint64 getTimeDiffEx(HRTick startTime, HRTick endTime, uint64& freq)
	{
		freq = m_freq;
		return endTime - startTime;
	}

	static HighResolutionTimer now();
	static HRTick getFrequency();

	static HRTick microsecondsToTicks(uint64 microseconds)
	{
		return microseconds * m_freq / 1000000;
	}

	static uint64 ticksToMicroseconds(HRTick ticks)
	{
		return ticks * 1000000 / m_freq;
	}

private:
	HighResolutionTimer(uint64 timePoint) : m_timePoint(timePoint) {};

	uint64 m_timePoint;
	static uint64 m_freq;
};

// benchmark helper utility
// measures time between Start() and Stop() call
class BenchmarkTimer
{
public:
	void Start()
	{
		m_startTime = HighResolutionTimer::now().getTick();
	}

	void Stop()
	{
		m_stopTime = HighResolutionTimer::now().getTick();
	}

	double GetElapsedMilliseconds() const
	{
		cemu_assert_debug(m_startTime != 0 && m_stopTime != 0);
		cemu_assert_debug(m_startTime <= m_stopTime);
		uint64 tickDif = m_stopTime - m_startTime;
		double freq = (double)HighResolutionTimer::now().getFrequency();
		double elapsedMS = (double)tickDif * 1000.0 / freq;
		return elapsedMS;
	}

private:
	HRTick m_startTime{};
	HRTick m_stopTime{};
};

