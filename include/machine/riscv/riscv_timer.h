// EPOS RISC-V Timer Mediator Declarations

#ifndef __riscv_timer_h
#define __riscv_timer_h

#include <architecture/cpu.h>
#include <machine/ic.h>
#include <machine/timer.h>
#include <system/memory_map.h>
#include <utility/convert.h>

__BEGIN_SYS

// Tick timer used by the system
class Timer : private Timer_Common, private CLINT
{
    friend Machine;
    friend IC;
    friend class Init_System;

protected:
    static const unsigned int CHANNELS = 2;
    static const Hertz FREQUENCY = Traits<Timer>::FREQUENCY;

    typedef IC_Common::Interrupt_Id Interrupt_Id;

public:
    using Timer_Common::Handler;
    using Timer_Common::Tick;

    // Channels
    enum
    {
        SCHEDULER,
        ALARM
    };

    static const Hertz CLOCK = Traits<Timer>::CLOCK;

protected:
    /**
     * @brief Constructs a Timer object.
     *
     * @param channel The channel for the timer (SCHEDULER or ALARM).
     * @param frequency The frequency for the timer.
     * @param handler The handler function for the timer interrupts.
     * @param retrigger Whether the timer should retrigger automatically.
     */
    Timer(unsigned int channel, Hertz frequency, Handler handler, bool retrigger = true) 
        : _channel(channel),
          _initial(FREQUENCY / frequency),
          _retrigger(retrigger),
          _handler(handler)
    {
        db<Timer>(TRC) << "Initializing Timer: frequency=" << frequency << ", handler=" 
                       << reinterpret_cast<void *>(handler) << ", channel=" << channel 
                       << ", initial count=" << _initial << endl;

        if (_initial && (channel < CHANNELS) && !_channels[channel])
        {
            _channels[channel] = this;
        }
        else
        {
            db<Timer>(TRC) << "Failed to install Timer on channel " << channel << endl;
        }

        for (unsigned int i = 0; i < Traits<Machine>::CPUS; i++)
        {
            _current[i] = _initial;
        }

        db<Timer>(TRC) << "Timer initialization complete for channel " << channel << endl;
    }

public:
    /**
     * @brief Destructs the Timer object.
     */
    ~Timer() {
        db<Timer>(TRC) << "Destroying Timer: frequency=" << frequency() << ", handler=" 
                       << reinterpret_cast<void *>(_handler) << ", channel=" << _channel 
                       << ", initial count=" << _initial << endl;

        _channels[_channel] = 0;
    }

    /**
     * @brief Reads the current tick count for the CPU.
     *
     * @return The current tick count.
     */
    Tick read() { return _current[CPU::id()]; }

    /**
     * @brief Restarts the timer and returns the percentage of time elapsed.
     *
     * @return The percentage of time elapsed since the timer was last started.
     */
    int restart() {
        int percentage = _current[CPU::id()] * 100 / _initial;
        _current[CPU::id()] = _initial;
        return percentage;
    }

    /**
     * @brief Resets the timer configuration.
     */
    static void reset() { config(FREQUENCY); }

    /**
     * @brief Enables the timer.
     */
    static void enable() {}

    /**
     * @brief Disables the timer.
     */
    static void disable() {}

    /**
     * @brief Gets the frequency of the timer.
     *
     * @return The timer frequency.
     */
    Hertz frequency() const { return (FREQUENCY / _initial); }

    /**
     * @brief Sets the frequency of the timer.
     *
     * @param f The new frequency.
     */
    void frequency(Hertz f)
    {
        _initial = FREQUENCY / f;
        reset();
    }

    /**
     * @brief Sets the handler function for the timer.
     *
     * @param handler The new handler function.
     */
    void handler(Handler handler) { _handler = handler; }

private:
    /**
     * @brief Configures the timer with the given frequency.
     *
     * @param frequency The frequency to configure.
     */
    static void config(Hertz frequency) 
    { 
        mtimecmp(mtime() + (CLOCK / frequency)); 
    }

    /**
     * @brief Handles timer interrupts.
     *
     * @param i The interrupt ID.
     */
    static void int_handler(Interrupt_Id i);

    /**
     * @brief Initializes the timer system.
     */
    static void init();

protected:
    unsigned int _channel;                 ///< Timer channel.
    Tick _initial;                         ///< Initial tick count.
    bool _retrigger;                       ///< Whether the timer should retrigger automatically.
    volatile Tick _current[Traits<Machine>::CPUS];  ///< Current tick count for each CPU.
    Handler _handler;                      ///< Handler function for timer interrupts.

    static Timer *_channels[CHANNELS];     ///< Timer channels.
};

// Timer used by Thread::Scheduler
class Scheduler_Timer : public Timer
{
public:
    /**
     * @brief Constructs a Scheduler_Timer object.
     *
     * @param quantum The time quantum for the scheduler.
     * @param handler The handler function for the scheduler timer.
     */
    Scheduler_Timer(Microsecond quantum, Handler handler) 
        : Timer(SCHEDULER, 1000000 / quantum, handler) 
    {
        db<Timer>(TRC) << "Scheduler_Timer initialized: quantum=" << quantum 
                       << ", initial count=" << 1000000 / quantum << endl;
    }
};

// Timer used by Alarm
class Alarm_Timer : public Timer
{
public:
    /**
     * @brief Constructs an Alarm_Timer object.
     *
     * @param handler The handler function for the alarm timer.
     */
    Alarm_Timer(Handler handler) : Timer(ALARM, FREQUENCY, handler) {}
};

__END_SYS

#endif
