TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        main.c

unix:!macx: LIBS += -lpthread

#system( g++ -o non-arbitration $$SOURCES -lpthread)
#system(time ./non-arbitration)
