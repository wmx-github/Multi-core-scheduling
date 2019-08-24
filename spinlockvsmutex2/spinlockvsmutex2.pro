TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        main.c

unix:!macx: LIBS += -lpthread


#message(COMPILE)
#system( gcc -o spin -DUSE_SPINLOCK $$SOURCES -lpthread)
#system( gcc -o mutex $$SOURCES -lpthread)

#message(RUN mutex)
#system(time ./mutex)

#message(RUN spin)
#system(time ./spin)
