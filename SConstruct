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
    'src/header.cc '
    'src/log.cc '
)

env.Append(CCFLAGS = ' -Wall -g')
env.Append(LIBS = [File('libev.a')])

env.StaticLibrary('ev', SOURCE)

env.Program('log_test', 'src/log_test.cc')

