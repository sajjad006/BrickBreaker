QT       += core gui widgets multimedia sql

CONFIG   += c++11
TARGET    = BrickBreaker
TEMPLATE  = app

SOURCES  += main.cpp \
            gamewidget.cpp \
            soundmanager.cpp \
            leaderboard.cpp \
            story.cpp

HEADERS  += gamewidget.h \
            soundmanager.h \
            leaderboard.h \
            story.h
