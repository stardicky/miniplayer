import QtQuick 2.7
import QtQuick.Window 2.2
import QtQuick.Controls 1.4
import IPTV 1.0

Window {
    visible: true
    width: 640
    height: 480
    title: "QtMiniPlayerTest"

    function formatTime(val) {
        if(val < 0)
            val = 0;
        var h = parseInt(val / 3600);
        var m = parseInt((val % 3600) / 60);
        var s = parseInt(val % 60);
        h = h < 10 ? '0' + h : h;
        m = m < 10 ? '0' + m : m;
        s = s < 10 ? '0' + s : s;
        return h + ':' + m + ":" + s;
    }

    function updateState() {
        txtState.text = formatState(ctlPlayer.state);
    }

    function formatState(state) {
        if(state === MiniPlayer.Stopped)
            return "Stopped";
        else if(state === MiniPlayer.Stopping)
            return "Stopping";
        else if(state === MiniPlayer.Opening)
            return "Opening";
        else if(state === MiniPlayer.Playing)
            return "Playing";
        else if(state === MiniPlayer.Paused)
            return "Paused";
        else
            return "";
    }

    Component.onCompleted: {
        updateState();
    }

    MiniPlayer {
        id: ctlPlayer
        onPositionChanged: {
            var position = ctlPlayer.position;
            var duration = ctlPlayer.duration;
            txtPosition.text = formatTime(position);
            txtDuration.text = formatTime(duration);
            ctlProgress.minimumValue = 0;
            ctlProgress.maximumValue = duration;

            if(!ctlProgress.pressed)
                ctlProgress.value = position;
        }
        onEndReached: {
            txtPosition.text = "00:00:00";
            ctlProgress.value = 0;
        }
        onStateChanged: {
            console.log("onStateChanged",formatState(ctlPlayer.state));
            updateState();
        }
    }

    VideoSurface {
        source: ctlPlayer
        anchors.fill: parent
    }

    Rectangle {
        radius: 10
        color: "#60000000"
        anchors.centerIn: parent;
        width: 100
        height: 100
        visible: ctlPlayer.buffering
        BusyIndicator {
            anchors.fill: parent
            anchors.margins: 30
            running: true
        }
        Text {
            id: txtDownloadSpeed
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 15
            horizontalAlignment: Text.horizontalAlignment
            color: "#ffffff"
        }
    }

//    VideoSurface {
//        source: ctlPlayer
//        anchors.left: parent.left
//        anchors.top: parent.top
//        width: parent.width * 0.25
//        height: parent.height * 0.25
//    }

//    VideoSurface {
//        source: ctlPlayer
//        anchors.right: parent.right
//        anchors.top: parent.top
//        width: parent.width * 0.25
//        height: parent.height * 0.25
//    }

//    VideoSurface {
//        source: ctlPlayer
//        anchors.left: parent.left
//        anchors.bottom: parent.bottom
//        width: parent.width * 0.25
//        height: parent.height * 0.25
//    }

//    VideoSurface {
//        source: ctlPlayer
//        anchors.right: parent.right
//        anchors.bottom: parent.bottom
//        width: parent.width * 0.25
//        height: parent.height * 0.25
//    }

    DumpInfo {
        id:dumpInfo
    }

    Timer {
        id: dumpTimer
        interval: 200
        running: true
        repeat: true
        onTriggered: {
            ctlPlayer.dump(dumpInfo);
            var downloadSpeed = (ctlPlayer.downloadSpeed / 1024).toFixed(1);
            txtDownloadSpeed.text = downloadSpeed + "KiB/s";
            txtStatus.text = "FPS:  " + ctlPlayer.fps + "\r\n" +
                        "D:    " + downloadSpeed + "KiB/s" + "\r\n" +
                        "V:    " + dumpInfo.videoClock.toFixed(3) + "\r\n" +
                        "A:    " + dumpInfo.audioClock.toFixed(3) + "\r\n" +
                        "A-V:  " + (dumpInfo.audioClock - dumpInfo.videoClock).toFixed(3) + "\r\n" +
                        "VPQ:  " + dumpInfo.videoPacketQueueSize + "\r\n" +
                        "APQ:  " + dumpInfo.audioPacketQueueSize + "\r\n" +
                        "VFQ:  " + dumpInfo.videoFrameQueueSize + "\r\n" +
                        "AFQ:  " + dumpInfo.audioFrameQueueSize + "\r\n" +
                        "VBD:  " + dumpInfo.videoBufferDuration.toFixed(3) + "\r\n" +
                        "ABD:  " + dumpInfo.audioBufferDuration.toFixed(3) + "\r\n" +
                        "BS:   " + dumpInfo.packetBufferSize + "/" + dumpInfo.maxPacketBufferSize;
        }
    }


    Rectangle {
        id: progressPannel
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        color: "#80000000"
        height: 30

        Text {
            id: txtPosition
            anchors.left: parent.left
            height: parent.height
            width: 60
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            color: "#ffffff"
            text: "00:00:00"
        }

        Slider {
            id:ctlProgress
            anchors.left: txtPosition.right
            anchors.right: txtDuration.left
            anchors.verticalCenter: parent.verticalCenter
            minimumValue: 0
            maximumValue: 0
            onPressedChanged: {
                if(!ctlProgress.pressed) {
                    ctlPlayer.position = ctlProgress.value;
                }
            }
        }

        Text {
            id: txtDuration
            anchors.right: txtVolume.left
            height: parent.height
            width: 60
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            color: "#ffffff"
            text: "00:00:00"
        }

        Text {
            id: txtVolume
            anchors.right: ctlVolume.left
            anchors.rightMargin: 10
            height: parent.height
            width: 50
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignRight
            color: "#ffffff"
            text: "Volume"
        }

        Slider {
            id: ctlVolume
            width: 80
            value: 1
            anchors.rightMargin: 10
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            minimumValue: 0
            maximumValue: 1
            onPressedChanged: {
                if(!ctlVolume.pressed) {
                    if(ctlPlayer.getMute())
                        ctlPlayer.unMute();
                    ctlPlayer.volume = ctlVolume.value;
                }
            }
        }
    }

    Rectangle {
        id: infoPanel
        anchors.right: parent.right
        anchors.bottom: progressPannel.top
        anchors.bottomMargin: 5
        anchors.rightMargin: 5
        color: "#80000000"
        height: 180
        width: 140

        Text {
            id: txtState
            anchors.left: parent.left
            anchors.right: parent.right
            padding: 5
            color: "#ffffff"
            wrapMode: Text.NoWrap
        }

        Text {
            id: txtStatus
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: txtState.bottom
            anchors.bottom: parent.bottom
            padding: 5
            color: "#ffffff"
        }
    }



    Button {
        id: btnOpen
        text: "Open"
        anchors.left: parent.left
        anchors.top: parent.tops
        onClicked: {
            //ctlPlayer.open("http://192.168.3.52:9001/1.ts");
            //ctlPlayer.open("F:\\TestVideos\\1.ts");
            ctlPlayer.open("http://192.168.3.52:9001/EP01.2015.HD720P.X264.AAC.Mandarin.CHS.mp4");
            //ctlPlayer.open("https://arch.p2sp.net/live");
            //ctlPlayer.open("https://arch.p2sp.net/ihdvod/EP01.2015.HD720P.X264.AAC.Mandarin.CHS.mp4");
            //ctlPlayer.open("https://arch.p2sp.net/tmp/new.ts");
            //ctlPlayer.open("http://192.168.3.38:8080");
        }
    }

    Button {
        id: btnStop
        text: "Stop"
        anchors.top: parent.top
        anchors.left: btnOpen.right
        onClicked: {
            ctlPlayer.stop();
        }
    }

    Button {
        id: btnPlay
        text: "Play"
        anchors.top: parent.top
        anchors.left: btnStop.right
        onClicked: {
            ctlPlayer.play();
        }
    }

    Button {
        id: btnPause
        text: "Pause"
        anchors.top: parent.top
        anchors.left: btnPlay.right
        onClicked: {
            ctlPlayer.pause();
        }
    }

    Button {
        id: btnTogglePause
        text: "TogglePause"
        anchors.top: parent.top
        anchors.left: btnPause.right
        onClicked: {
            ctlPlayer.togglePause();
        }
    }

    Button {
        id: btnMute
        text: "Mute"
        anchors.top: parent.top
        anchors.left: btnTogglePause.right
        onClicked: {
            ctlPlayer.mute();
        }
    }

    Button {
        id: btnUnMute
        text: "UnMute"
        anchors.top: parent.top
        anchors.left: btnMute.right
        onClicked: {
            ctlPlayer.unMute();
        }
    }

    Button {
        id: btnToggleMute
        text: "ToggleMute"
        anchors.top: parent.top
        anchors.left: btnUnMute.right
        onClicked: {
            ctlPlayer.toggleMute();
        }
    }
}
