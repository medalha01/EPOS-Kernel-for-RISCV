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
    Timer(unsigned int channel, Hertz frequency, Handler handler, bool retrigger = true) 
        : _channel(channel),
          _initial(FREQUENCY / frequency),
          _retrigger(retrigger),
          _handler(handler)
    {
        db<Timer>(INF) << "Initializing Timer: channel = " << channel
                       << ", frequency = " << frequency
                       << ", handler = " << reinterpret_cast<void *>(handler)
                       << ", retrigger = " << retrigger << endl;

        if (_initial && (channel < CHANNELS) && !_channels[channel])
        {
            db<Timer>(INF) << "Setting up timer channel " << channel << " with initial count " << _initial << endl;
            _channels[channel] = this;
        }
        else
        {
            db<Timer>(WRN) << "Failed to install Timer on channel " << channel << endl;
        }

        for (unsigned int i = 0; i < Traits<Machine>::CPUS; i++)
        {
            db<Timer>(TRC) << "Initializing current tick for CPU " << i << " with initial count " << _initial << endl;
            _current[i] = _initial;
        }

        db<Timer>(INF) << "Timer initialization complete for channel " << channel << endl;
    }

public:
    ~Timer() {
        db<Timer>(TRC) << "Destroying Timer: frequency = " << frequency()
                       << ", handler = " << reinterpret_cast<void *>(_handler)
                       << ", channel = " << _channel
                       << ", initial count = " << _initial << endl;

        _channels[_channel] = nullptr;
    }

    Tick read() { 
        return _current[CPU::id()]; 
    }

    int restart() {
        db<Timer>(INF) << "Restarting Timer: current tick = " << _current[CPU::id()] 
                       << ", initial count = " << _initial << endl;

        int percentage = _current[CPU::id()] * 100 / _initial;

        db<Timer>(TRC) << "Computed percentage: " << percentage << "%" << endl;

        _current[CPU::id()] = _initial;

        db<Timer>(INF) << "Reset current tick for CPU " << CPU::id() << " to initial count " << _initial << endl;

        return percentage;
    }

    static void reset() { 
        config(FREQUENCY); 
    }
    static void enable() {
    }
    static void disable() {
    }

    Hertz frequency() const { 
        return (FREQUENCY / _initial); 
    }
    void frequency(Hertz f) {
        _initial = FREQUENCY / f;
        reset();
    }

    void handler(Handler handler) { 
        _handler = handler; 
    }

private:
    static void config(Hertz frequency) { 
		mtimecmp(mtime() + (CLOCK / frequency));
    }

    static void int_handler(Interrupt_Id i);

    static void init();

protected:
    unsigned int _channel;
    Tick _initial;
    bool _retrigger;
    volatile Tick _current[Traits<Machine>::CPUS];
    Handler _handler;

    static Timer *_channels[CHANNELS];
};

// Timer used by Thread::Scheduler
class Scheduler_Timer : public Timer
{
public:
    Scheduler_Timer(Microsecond quantum, Handler handler) 
        : Timer(SCHEDULER, 1000000 / quantum, handler) 
    {
        db<Scheduler_Timer>(INF) << "Initializing Scheduler_Timer: quantum = " << quantum
                                 << ", initial count = " << 1000000 / quantum << endl;
    }
};

// Timer used by Alarm
class Alarm_Timer : public Timer
{
public:
    Alarm_Timer(Handler handler) 
        : Timer(ALARM, FREQUENCY, handler) 
    {
        db<Alarm_Timer>(INF) << "Initializing Alarm_Timer with frequency " << FREQUENCY << endl;
    }
};

__END_SYS

#endif
