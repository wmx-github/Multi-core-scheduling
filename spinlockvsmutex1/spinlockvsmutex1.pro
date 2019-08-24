TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        main.cc

unix:!macx: LIBS += -lpthread



#message(COMPILE)
#system( g++ -o spin_version -DUSE_SPINLOCK $$SOURCES -lpthread)
#system( g++ -o mutex_version $$SOURCES -lpthread)

#message(RUN mutex_version)
#system(time ./mutex_version)

#message(RUN spin_version)
#system(time ./spin_version)

