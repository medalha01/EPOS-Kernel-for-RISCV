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
		//db<Thread>(WRN) << "@@@TIMER CONS@@@ frequency = " << frequency  << " - --- -AAAAAAAAAA" << endl;
		//db<Thread>(WRN) << "@@@TIMER CONS@@@ FREQUENCY / frequency = " << FREQUENCY / frequency << endl;

		//db<Thread>(WRN) << "--Timer(f=" << frequency
		//	<< ",h=" << reinterpret_cast<void *>(handler)
		//	<< ",ch=" << channel << ") => {count=" << _initial << "}"
		//	<< endl;

		if (_initial && (channel < CHANNELS) && !_channels[channel])
		{
			db<Thread>(WRN) << "timer: no if _initial && channel\n\n" << endl;
			_channels[channel] = this;
		}
		else
		{
			db<Thread>(WRN) << "timer: NO ELSE\n\n" << endl;
			db<Timer>(WRN) << "Timer not installed!" << endl;
		}

		for (unsigned int i = 0; i < Traits<Machine>::CPUS; i++)
		{
			db<Thread>(WRN) << "timer: current[" << i << "] = initial\n\n" << endl;
			_current[i] = _initial;
		}

		db<Thread>(WRN) << "\n\ntimer:cabou" << endl;
	}

public:
	~Timer() {
		db<Timer>(TRC) << "~Timer(f=" << frequency()
			<< ",h=" << reinterpret_cast<void *>(_handler)
			<< ",ch=" << _channel << ") => {count=" << _initial
			<< "}" << endl;

		_channels[_channel] = 0;
	}

    Tick read() { return _current[CPU::id()]; }

	int restart() {
		// TODO: change to <Timer> again
		//
		//db<Thread>(WRN) << "@@@TIMER _initialjkfldsajfldsajfedsjlkfdsajlkfdsajlkfdsajlksafdjlflsdaj" << endl; 
		//db<Thread>(WRN) << "@@@TIMER _initial = " << _initial << endl;
		//db<Thread>(WRN) << "@@@TIMER FREQUENCY = " << FREQUENCY << endl;

		//db<Thread>(WRN) << "@@@TIMER::restart() => {f=" << frequency() << endl;
			//<< ",h=" << reinterpret_cast<void *>(_handler)
			//<< ",count=" << _current[CPU::id()] << "}" << endl;

		//db<Thread>(WRN) << "@@@TIMER: ANTES DO PERCENTAGE" << endl;

		int percentage = _current[CPU::id()] * 100 / _initial;

		//db<Thread>(WRN) << "@@@TIMER: LOGO DEPOIS DO PERCENTAGE" << endl;
		
		_current[CPU::id()] = _initial;

		//db<Thread>(WRN) << "@@@TIMER: LOGO DEPOIS DE SETAR O CPU_ID = " << _initial << endl;

		return percentage;
	}

    static void reset() { config(FREQUENCY); }
    static void enable() {}
    static void disable() {}

    Hertz frequency() const { return (FREQUENCY / _initial); }
    void frequency(Hertz f)
    {
        _initial = FREQUENCY / f;
        reset();
    }

    void handler(Handler handler) { _handler = handler; }

private:
    static void config(Hertz frequency) 
	{ 
		//db<Thread>(WRN) << "TIMER config antes do mtimecmp" << endl;
		mtimecmp(mtime() + (CLOCK / frequency)); 
		//db<Thread>(WRN) << "TIMER config depois do mtimecmp" << endl;
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
    Scheduler_Timer(Microsecond quantum, Handler handler) : Timer(SCHEDULER, 1000000 / quantum, handler) 
	{
		db<Thread>(WRN) << "@@@SCHEDTIMER@@@ quantum = " << quantum << endl;
		db<Thread>(WRN) << "@@@SCHEDTIMER@@@ _initial = " << 1000000 / quantum << endl;
	}
};

// Timer used by Alarm
class Alarm_Timer : public Timer
{
public:
    Alarm_Timer(Handler handler) : Timer(ALARM, FREQUENCY, handler) {}
};

__END_SYS

#endif
