env=Environment()
env=env.Clone()

if not env.GetOption('clean'):
    conf = Configure(env)
    conf.CheckCC()
    conf.CheckCXX()
    if conf.CheckCHeader('sys/eventfd.h'):
        has_sys_eventfd_h = True
        env.Append(CPPFLAGS = ' -DHAVE_SYS_EVENTFD')
    if conf.CheckCHeader('sys/signalfd.h'):
        has_sys_signalfd_h = True
        env.Append(CPPFLAGS = ' -DHAVE_SYS_SIGNALFD')
    if conf.CheckCHeader('sys/timerfd.h'):
        has_sys_timerfd_h = True
        env.Append(CPPFLAGS = ' -DHAVE_SYS_TIMERFD')
    env = conf.Finish()

SOURCE=Split(
    'src/ev.cc '
    'src/interrupter.cc '
    'src/log.cc '
    'src/reactor.cc '
)

env.Append(CCFLAGS = ' -Wall -g -std=c++98')
env.Append(LIBS = [File('libev.a'), 'pthread', 'rt'])

#env.SharedLibrary('ev', SOURCE, LINKFLAGS='-Wl,--no-undefined')
env.StaticLibrary('ev', SOURCE)

env.Program('log_test',                 'src/log_test.cc')
env.Program('interrupter_test',         'src/interrupter_test.cc')
env.Program('io_test',                  'src/io_test.cc')
env.Program('http_get_test',            'src/http_get_test.cc')
env.Program('signal_test',              'src/signal_test.cc')
env.Program('timer_test',               'src/timer_test.cc')

