import QtQuick
import QtQuick.Dialogs

// Lazy-loaded native OS dialogs so QtQuick.Dialogs stays unloaded until user opens a file/folder picker
Item {
    id: root

    required property var playerCtrl
    required property var libCtrl
    required property var onlineCtrl
    property int exportPlaylistTargetId: -1

    function openByName(name) {
        if (name === "openAudio") openAudioDialog.open()
        else if (name === "addFolder") addFolderDialog.open()
        else if (name === "selectDownloadDir") selectDownloadDirDialog.open()
        else if (name === "selectNeteaseServiceDir") selectNeteaseServiceDirDialog.open()
        else if (name === "selectMetingServiceDir") selectMetingServiceDirDialog.open()
        else if (name === "importM3u") importM3uDialog.open()
        else if (name === "exportM3u") exportM3uDialog.open()
    }

    FileDialog {
        id: openAudioDialog
        title: qsTr("Open Audio Files")
        fileMode: FileDialog.OpenFiles
        nameFilters: [
            qsTr("Audio & Playlists (*.flac *.wav *.aiff *.alac *.ape *.wv *.tta *.mp3 *.m4a *.aac *.ogg *.opus *.wma *.dsf *.dff *.cue *.m3u *.m3u8)"),
            qsTr("All Files (*)")
        ]
        onAccepted: root.playerCtrl.openUrls(selectedFiles, false)
    }

    FolderDialog {
        id: addFolderDialog
        title: qsTr("Select Music Folder")
        onAccepted: root.libCtrl.addWatchedFolder(selectedFolder)
    }

    FolderDialog {
        id: selectDownloadDirDialog
        title: qsTr("Select Online Download Folder")
        onAccepted: root.onlineCtrl.setDownloadDirFromUrl(selectedFolder)
    }

    FolderDialog {
        id: selectNeteaseServiceDirDialog
        title: qsTr("Select Netease API Service Folder")
        onAccepted: root.onlineCtrl.setNeteaseServiceDirFromUrl(selectedFolder)
    }

    FolderDialog {
        id: selectMetingServiceDirDialog
        title: qsTr("Select Meting API Service Folder")
        onAccepted: root.onlineCtrl.setMetingServiceDirFromUrl(selectedFolder)
    }

    FileDialog {
        id: importM3uDialog
        title: qsTr("Import M3U / M3U8 Playlist")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("M3U Playlists (*.m3u *.m3u8)"), qsTr("All Files (*)")]
        onAccepted: root.libCtrl.importM3uPlaylist(selectedFile)
    }

    FileDialog {
        id: exportM3uDialog
        title: qsTr("Export M3U8 Playlist")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "m3u8"
        nameFilters: [qsTr("M3U8 Playlist (*.m3u8)")]
        onAccepted: {
            if (root.exportPlaylistTargetId >= 0) {
                root.libCtrl.exportM3uPlaylist(root.exportPlaylistTargetId, selectedFile)
            }
        }
    }
}
