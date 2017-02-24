TEMPLATE = app

QT += qml quick
CONFIG += c++14

SOURCES += \
    src/Main.cpp \
    src/miniplayer/MiniPlayer.cpp \
    src/miniplayer/output/audio/AudioOutputOpenAL.cpp \
    src/miniplayer/qt/QmlMiniPlayer.cpp \
    src/miniplayer/qt/QmlVideoSurface.cpp \
    src/miniplayer/qt/SGVideoNode.cpp

RESOURCES += qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

INCLUDEPATH += \
    3rdparty/ffmpeg-3.2.2/include \
    3rdparty/OpenAL-1.1/include \

LIBS += \
    $$PWD/3rdparty/ffmpeg-3.2.2/lib/avformat.lib \
    $$PWD/3rdparty/ffmpeg-3.2.2/lib/avcodec.lib \
    $$PWD/3rdparty/ffmpeg-3.2.2/lib/avutil.lib \
    $$PWD/3rdparty/ffmpeg-3.2.2/lib/swresample.lib \
    $$PWD/3rdparty/OpenAL-1.1/libs/Win32/OpenAL32.lib \

HEADERS += \
    src/miniplayer/MiniPlayer.hpp \
    src/miniplayer/Queue.hpp \
    src/miniplayer/Command.hpp \
    src/miniplayer/output/audio/AudioOutput.hpp \
    src/miniplayer/output/audio/AudioOutputOpenAL.hpp \
    src/miniplayer/qt/QmlMiniPlayer.hpp \
    src/miniplayer/qt/QmlVideoSurface.hpp \
    src/miniplayer/qt/QVideoFrame.hpp \
    src/miniplayer/qt/SGVideoNode.hpp
