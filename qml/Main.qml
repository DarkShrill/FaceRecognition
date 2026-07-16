pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import VideoStream 1.0

ApplicationWindow {
    id: root
    width: 1180
    height: 780
    minimumWidth: 920
    minimumHeight: 640
    visible: true
    title: "Face Recognition Studio"
    color: colors.background
    font.family: root.appFontFamily

    readonly property string appFontFamily: poppinsRegular.status === FontLoader.Ready ? poppinsRegular.name : "Poppins"
    readonly property var controller: faceController
    readonly property int recognitionTab: 0
    readonly property int addFaceTab: 1
    readonly property int loadFaceTab: 2
    readonly property int embeddingsTab: 3
    property bool splitOverlayView: false
    property int currentTab: 0
    property bool recognitionPlayerActive: false

    QtObject {
        id: colors
        readonly property color background: "#0b0d10"
        readonly property color backgroundTop: "#172027"
        readonly property color surface: "#14181d"
        readonly property color surfaceRaised: "#1b2026"
        readonly property color surfaceSoft: "#222832"
        readonly property color line: "#303741"
        readonly property color lineStrong: "#3e4853"
        readonly property color text: "#f4f7f8"
        readonly property color muted: "#a8b0ba"
        readonly property color dim: "#6f7a85"
        readonly property color accent: "#65d6c6"
        readonly property color accentStrong: "#35bfae"
        readonly property color accentSoft: "#1f4f4a"
        readonly property color switchBlue: "#2f66f6"
        readonly property color switchBluePressed: "#2858d8"
        readonly property color navBackground: "#11161d"
        readonly property color navActive: "#1f2732"
        readonly property color navHover: "#18202a"
        readonly property color navText: "#f7f9fc"
        readonly property color navMuted: "#a7b0be"
        readonly property color navLine: "#2c3643"
        readonly property color warning: "#f4b860"
        readonly property color danger: "#ff7676"
        readonly property color violet: "#b99cff"
    }

    FontLoader {
        id: poppinsRegular
        source: "qrc:/fonts/Poppins-Regular.ttf"
    }

    FontLoader {
        id: poppinsMedium
        source: "qrc:/fonts/Poppins-Medium.ttf"
    }

    FontLoader {
        id: poppinsSemiBold
        source: "qrc:/fonts/Poppins-SemiBold.ttf"
    }

    function ensureControllerRunning() {
        if (root.controller && !root.controller.running) {
            root.controller.videoUrl = videoUrlField.text
            root.controller.start()
        }
    }

    function startRecognitionPlayer() {
        if (!root.recognitionPlayerActive) {
            player.start(root.controller.videoUrl)
            root.recognitionPlayerActive = true
        }
    }

    function stopRecognitionPlayer() {
        if (root.recognitionPlayerActive) {
            player.stop()
            root.recognitionPlayerActive = false
        }
    }

    function syncPlayersForTab() {
        if (!root.controller) {
            return
        }

        stopRecognitionPlayer()
        if (root.currentTab !== root.recognitionTab && root.currentTab !== root.addFaceTab) {
            return
        }
        ensureControllerRunning()
        startRecognitionPlayer()
    }

    Component.onCompleted: {
        if (root.controller) {
            root.currentTab = 0
            tabs.currentIndex = 0
            syncPlayersForTab()
        }
    }

    component AppTextField: TextField {
        id: field
        property string labelText: ""
        property bool prominent: false
        color: colors.text
        placeholderTextColor: colors.dim
        selectedTextColor: colors.background
        selectionColor: colors.accent
        font.family: root.appFontFamily
        font.pixelSize: 14
        leftPadding: 14
        rightPadding: 14
        topPadding: field.labelText.length > 0 ? 19 : 8
        bottomPadding: field.labelText.length > 0 ? 5 : 8
        background: Rectangle {
            implicitHeight: field.labelText.length > 0 ? 50 : 38
            radius: 8
            color: field.enabled ? (field.prominent ? "#242c37" : colors.surfaceRaised) : "#111418"
            border.width: 1
            border.color: field.activeFocus ? colors.switchBlue : field.prominent ? colors.lineStrong : colors.line

            Label {
                anchors.left: parent.left
                anchors.leftMargin: field.leftPadding
                anchors.top: parent.top
                anchors.topMargin: 5
                text: field.labelText
                visible: text.length > 0
                color: field.activeFocus ? colors.switchBlue : colors.muted
                font.family: root.appFontFamily
                font.pixelSize: 10
                font.weight: Font.DemiBold
            }
        }
    }

    component AppButton: Button {
        id: button
        property bool primary: false
        property bool danger: false
        property bool compact: false
        font.family: root.appFontFamily
        font.pixelSize: compact ? 13 : 14
        font.bold: primary
        leftPadding: compact ? 12 : 16
        rightPadding: compact ? 12 : 16
        topPadding: compact ? 7 : 9
        bottomPadding: compact ? 7 : 9
        contentItem: Text {
            text: button.text
            color: !button.enabled ? colors.dim : button.primary ? colors.background : button.danger ? colors.danger : colors.text
            font: button.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            implicitHeight: button.compact ? 34 : 40
            radius: 8
            color: !button.enabled ? "#171b20" :
                   button.primary ? (button.down ? colors.accentStrong : colors.accent) :
                   button.down ? colors.surfaceSoft : colors.surfaceRaised
            border.width: button.primary ? 0 : 1
            border.color: button.danger ? "#6a343a" : button.hovered ? colors.lineStrong : colors.line
        }
    }

    component AppComboBox: ComboBox {
        id: combo
        font.family: root.appFontFamily
        font.pixelSize: 14
        contentItem: Text {
            text: combo.displayText
            color: combo.enabled ? colors.text : colors.dim
            font: combo.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            leftPadding: 14
            rightPadding: 28
        }
        indicator: Canvas {
            x: combo.width - width - 12
            y: combo.topPadding + (combo.availableHeight - height) / 2
            width: 10
            height: 6
            contextType: "2d"
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.beginPath()
                ctx.moveTo(0, 0)
                ctx.lineTo(width, 0)
                ctx.lineTo(width / 2, height)
                ctx.closePath()
                ctx.fillStyle = combo.enabled ? colors.muted : colors.dim
                ctx.fill()
            }
        }
        background: Rectangle {
            implicitHeight: 38
            radius: 8
            color: combo.enabled ? colors.surfaceRaised : "#111418"
            border.width: 1
            border.color: combo.activeFocus || combo.down ? colors.accent : colors.line
        }
        popup: Popup {
            y: combo.height + 6
            width: combo.width
            implicitHeight: Math.min(contentItem.implicitHeight + 12, 260)
            padding: 6
            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: combo.popup.visible ? combo.delegateModel : null
                currentIndex: combo.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator { }
            }
            background: Rectangle {
                radius: 8
                color: colors.surfaceRaised
                border.color: colors.lineStrong
            }
        }
        delegate: ItemDelegate {
            id: comboDelegate
            required property int index
            required property var modelData
            width: combo.width - 12
            height: 34
            highlighted: combo.highlightedIndex === comboDelegate.index
            contentItem: Text {
                text: comboDelegate.modelData.text
                color: comboDelegate.highlighted ? colors.background : colors.text
                font.family: root.appFontFamily
                font.pixelSize: 14
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            background: Rectangle {
                radius: 6
                color: comboDelegate.highlighted ? colors.accent : "transparent"
            }
        }
    }

    component StatPill: Rectangle {
        id: pill
        property string label: ""
        property string value: ""
        property color accentColor: colors.accent
        height: 34
        radius: 8
        color: colors.surfaceRaised
        border.color: colors.line
        implicitWidth: pillRow.implicitWidth + 24

        RowLayout {
            id: pillRow
            anchors.centerIn: parent
            spacing: 8

            Rectangle {
                Layout.preferredWidth: 7
                Layout.preferredHeight: 7
                radius: 4
                color: pill.accentColor
            }

            Label {
                text: pill.label
                color: colors.muted
                font.pixelSize: 12
            }

            Label {
                text: pill.value
                color: colors.text
                font.pixelSize: 13
                font.bold: true
            }
        }
    }

    component AppTabButton: Button {
        id: tabButton
        property int tabIndex: -1
        checkable: false
        checked: tabs.currentIndex === tabButton.tabIndex
        hoverEnabled: true
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Layout.preferredHeight: 46
        implicitHeight: 46
        topPadding: 0
        bottomPadding: 0
        leftPadding: 14
        rightPadding: 14
        onClicked: {
            tabs.currentIndex = tabButton.tabIndex
        }
        contentItem: Text {
            text: tabButton.text
            color: tabButton.checked ? colors.navText : colors.navMuted
            font.family: root.appFontFamily
            font.pixelSize: 14
            font.weight: tabButton.checked ? Font.DemiBold : Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 12
            color: tabButton.checked ? colors.navActive : tabButton.hovered ? colors.navHover : "transparent"
            border.width: tabButton.checked ? 1 : 0
            border.color: colors.navLine

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 4
                width: Math.min(52, parent.width * 0.38)
                height: 3
                radius: 2
                color: colors.switchBlue
                visible: tabButton.checked
            }
        }
    }

    component Panel: Rectangle {
        radius: 8
        color: colors.surface
        border.width: 1
        border.color: colors.line
        clip: true
    }

    background: Rectangle {
        color: colors.background

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: colors.backgroundTop }
                GradientStop { position: 0.52; color: colors.background }
                GradientStop { position: 1.0; color: "#090a0c" }
            }
        }

        Rectangle {
            x: parent.width * 0.58
            y: -140
            width: 460
            height: 260
            radius: 8
            rotation: -8
            color: Qt.rgba(0.39, 0.84, 0.78, 0.10)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                Label {
                    text: "Face Recognition Studio"
                    color: colors.text
                    font.pixelSize: 25
                    font.weight: Font.DemiBold
                }

                Label {
                    text: root.controller ? root.controller.status : "Initializing"
                    color: root.controller && root.controller.lowLight ? colors.warning : colors.muted
                    font.pixelSize: 13
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            AppTextField {
                id: videoUrlField
                text: root.controller ? root.controller.videoUrl : "video=Full HD webcam"
                placeholderText: "video=Integrated Camera or rtsp://..."
                Layout.preferredWidth: 330
                onEditingFinished: {
                    if (root.controller) {
                        root.controller.videoUrl = text
                    }
                }
            }

            AppComboBox {
                id: inferenceDeviceSelector
                enabled: root.controller !== null
                model: [
                    { text: "CPU", value: 0 },
                    { text: "CUDA", value: 1 }
                ]
                textRole: "text"
                valueRole: "value"
                currentIndex: root.controller ? root.controller.inferenceDevice : 0
                displayText: "Inference: " + currentText
                Layout.preferredWidth: 154
                onActivated: function(index) {
                    if (root.controller) {
                        root.controller.inferenceDevice = inferenceDeviceSelector.valueAt(index)
                    }
                }

                Connections {
                    target: root.controller
                    function onInferenceDeviceChanged() {
                        inferenceDeviceSelector.currentIndex = inferenceDeviceSelector.indexOfValue(root.controller.inferenceDevice)
                    }
                }
            }

            AppButton {
                text: root.controller && root.controller.running ? "Stop" : "Start"
                primary: root.controller && !root.controller.running
                enabled: root.controller !== null
                Layout.preferredWidth: 96
                onClicked: {
                    if (root.controller.running) {
                        root.stopRecognitionPlayer()
                        root.controller.stop()
                    } else {
                        root.ensureControllerRunning()
                        root.syncPlayersForTab()
                    }
                }
            }
        }

        Rectangle {
            id: tabs
            property int currentIndex: root.currentTab
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            color: colors.navBackground
            radius: 15
            border.color: colors.navLine
            border.width: 1
            onCurrentIndexChanged: {
                if (root.currentTab !== currentIndex) {
                    root.currentTab = currentIndex
                    root.syncPlayersForTab()
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 6

                AppTabButton {
                    text: "Recognition"
                    tabIndex: root.recognitionTab
                }

                AppTabButton {
                    text: "Add face"
                    tabIndex: root.addFaceTab
                }

                AppTabButton {
                    text: "Load face"
                    tabIndex: root.loadFaceTab
                }

                AppTabButton {
                    text: "Embeddings"
                    tabIndex: root.embeddingsTab
                }
            }
        }

        Panel {
            Layout.fillWidth: true
            Layout.preferredHeight: 74

            StackLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                currentIndex: root.currentTab

                Item {
                    RowLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 14

                        Switch {
                            id: splitOverlaySwitch
                            text: "Split overlay"
                            checked: root.splitOverlayView
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredHeight: 38
                            onToggled: root.splitOverlayView = checked
                            indicator: Rectangle {
                                implicitWidth: 48
                                implicitHeight: 28
                                x: splitOverlaySwitch.leftPadding
                                y: splitOverlaySwitch.height / 2 - height / 2
                                radius: height / 2
                                color: splitOverlaySwitch.checked ? (splitOverlaySwitch.down ? colors.switchBluePressed : colors.switchBlue) : colors.surfaceSoft
                                border.width: splitOverlaySwitch.checked ? 0 : 1
                                border.color: colors.lineStrong

                                Behavior on color {
                                    ColorAnimation { duration: 130 }
                                }

                                Rectangle {
                                    x: switchThumb.x + 1
                                    y: switchThumb.y + 2
                                    width: switchThumb.width
                                    height: switchThumb.height
                                    radius: switchThumb.radius
                                    color: "#26000000"
                                    visible: splitOverlaySwitch.checked
                                }

                                Rectangle {
                                    id: switchThumb
                                    x: splitOverlaySwitch.checked ? parent.width - width - 3 : 3
                                    y: 3
                                    width: 22
                                    height: 22
                                    radius: width / 2
                                    color: "#ffffff"
                                    border.width: splitOverlaySwitch.checked ? 0 : 1
                                    border.color: "#d8dee8"

                                    Behavior on x {
                                        NumberAnimation {
                                            duration: 150
                                            easing.type: Easing.OutCubic
                                        }
                                    }
                                }
                            }
                            contentItem: Text {
                                text: splitOverlaySwitch.text
                                color: colors.text
                                font.family: root.appFontFamily
                                font.pixelSize: 14
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: splitOverlaySwitch.indicator.width + 12
                            }
                        }

                        CheckBox {
                            id: autoSaveCheck
                            text: "Salva volti riconosciuti"
                            enabled: root.controller !== null
                            checked: false
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: {
                                if (root.controller) {
                                    root.controller.setAutoSaveDetectedFaces(checked)
                                }
                            }
                            indicator: Rectangle {
                                implicitWidth: 22
                                implicitHeight: 22
                                x: autoSaveCheck.leftPadding
                                y: autoSaveCheck.height / 2 - height / 2
                                radius: 6
                                color: autoSaveCheck.checked ? colors.accent : colors.surfaceRaised
                                border.color: autoSaveCheck.checked ? colors.accent : colors.lineStrong

                                Canvas {
                                    anchors.fill: parent
                                    visible: autoSaveCheck.checked
                                    contextType: "2d"
                                    onVisibleChanged: requestPaint()
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.clearRect(0, 0, width, height)
                                        ctx.strokeStyle = colors.background
                                        ctx.lineWidth = 2.4
                                        ctx.lineCap = "round"
                                        ctx.lineJoin = "round"
                                        ctx.beginPath()
                                        ctx.moveTo(width * 0.28, height * 0.52)
                                        ctx.lineTo(width * 0.44, height * 0.68)
                                        ctx.lineTo(width * 0.73, height * 0.34)
                                        ctx.stroke()
                                    }
                                }
                            }
                            contentItem: Text {
                                text: autoSaveCheck.text
                                color: autoSaveCheck.enabled ? colors.text : colors.dim
                                font.family: root.appFontFamily
                                font.pixelSize: 14
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: autoSaveCheck.indicator.width + 10
                            }
                        }

                        AppComboBox {
                            id: landmarkSelector
                            enabled: root.controller !== null
                            model: [
                                { text: "Landmarks: off", value: 0 },
                                { text: "Landmarks: 5 points", value: 1 },
                                { text: "Landmarks: 106", value: 2 },
                                { text: "Landmarks: all", value: 3 },
                                { text: "Landmarks: 3D 68", value: 4 }
                            ]
                            textRole: "text"
                            valueRole: "value"
                            currentIndex: root.controller ? root.controller.landmarkMode : 3
                            Layout.preferredWidth: 190
                            Layout.alignment: Qt.AlignVCenter
                            onActivated: function(index) {
                                if (root.controller) {
                                    root.controller.landmarkMode = landmarkSelector.valueAt(index)
                                }
                            }

                            Connections {
                                target: root.controller
                                function onLandmarkModeChanged() {
                                    landmarkSelector.currentIndex = landmarkSelector.indexOfValue(root.controller.landmarkMode)
                                }
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                    }
                }

                Item {
                    RowLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 12

                        AppTextField {
                            id: personNameField
                            labelText: "Person"
                            prominent: true
                            placeholderText: "Name or surname"
                            Layout.preferredWidth: 260
                            Layout.alignment: Qt.AlignVCenter
                            enabled: root.controller && !root.controller.enrolling && !root.controller.extracting
                        }

                        Label {
                            text: "15 guided photos"
                            color: colors.muted
                            font.family: root.appFontFamily
                            font.pixelSize: 14
                            Layout.alignment: Qt.AlignVCenter
                        }

                        AppButton {
                            text: root.controller && (root.controller.enrolling || root.controller.extracting) ? "Cancel" : "Capture"
                            primary: root.controller && !(root.controller.enrolling || root.controller.extracting)
                            enabled: root.controller !== null
                            Layout.preferredWidth: 104
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: {
                                if (root.controller.enrolling || root.controller.extracting) {
                                    root.controller.cancelFaceEnrollment()
                                } else {
                                    root.controller.startFaceEnrollment(personNameField.text)
                                }
                            }
                        }

                        ProgressBar {
                            id: captureProgress
                            from: 0
                            to: root.controller ? Math.max(1, root.controller.enrollmentTargetCount) : 1
                            value: root.controller ? root.controller.enrollmentCapturedCount : 0
                            indeterminate: root.controller && root.controller.extracting
                            Layout.preferredWidth: 180
                            Layout.alignment: Qt.AlignVCenter
                            background: Rectangle {
                                implicitHeight: 8
                                radius: 4
                                color: colors.surfaceSoft
                            }
                            contentItem: Item {
                                implicitHeight: 8

                                Rectangle {
                                    id: captureProgressFill
                                    width: captureProgress.indeterminate ? parent.width * 0.36 : captureProgress.visualPosition * parent.width
                                    height: parent.height
                                    radius: 4
                                    color: colors.accent

                                    NumberAnimation on x {
                                        from: -captureProgressFill.width
                                        to: captureProgressFill.parent ? captureProgressFill.parent.width : 0
                                        duration: 1100
                                        loops: Animation.Infinite
                                        running: captureProgress.indeterminate
                                    }
                                }
                            }
                            onIndeterminateChanged: {
                                if (!indeterminate) {
                                    captureProgressFill.x = 0
                                }
                            }
                        }

                        Label {
                            text: root.controller ? root.controller.enrollmentStatus : "Ready"
                            color: colors.muted
                            font.pixelSize: 14
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }
                }

                Item {
                    RowLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 12

                        AppTextField {
                            id: loadFaceNameField
                            labelText: "Person"
                            prominent: true
                            placeholderText: "Name or surname"
                            Layout.preferredWidth: 240
                            Layout.alignment: Qt.AlignVCenter
                            enabled: root.controller && !root.controller.enrolling && !root.controller.extracting
                        }

                        AppTextField {
                            id: loadFaceFolderField
                            labelText: "Folder path"
                            prominent: true
                            placeholderText: "C:/path/to/folder"
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            enabled: root.controller && !root.controller.enrolling && !root.controller.extracting
                            onAccepted: {
                                if (loadFaceButton.enabled) {
                                    root.controller.learnFaceFromFolder(loadFaceNameField.text, loadFaceFolderField.text)
                                }
                            }
                        }

                        AppButton {
                            id: loadFaceButton
                            text: root.controller && root.controller.extracting ? "Loading..." : "Load"
                            primary: true
                            enabled: root.controller &&
                                     !root.controller.enrolling &&
                                     !root.controller.extracting &&
                                     loadFaceNameField.text.trim().length > 0 &&
                                     loadFaceFolderField.text.trim().length > 0
                            Layout.preferredWidth: 90
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: root.controller.learnFaceFromFolder(loadFaceNameField.text, loadFaceFolderField.text)
                        }

                        Label {
                            text: root.controller ? root.controller.enrollmentStatus : "Ready"
                            color: colors.muted
                            font.pixelSize: 14
                            elide: Text.ElideRight
                            Layout.preferredWidth: 260
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }
                }

                Item {
                    RowLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 12

                        Label {
                            text: root.controller ? (root.controller.knownFaces.length + " embedding files") : "0 embedding files"
                            color: colors.text
                            font.pixelSize: 15
                            font.weight: Font.Medium
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                        }

                        AppButton {
                            text: "Refresh"
                            compact: true
                            enabled: root.controller !== null
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: root.controller.refreshKnownFaces()
                        }
                    }
                }
            }
        }

        Panel {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            color: colors.surfaceRaised
            visible: root.currentTab === root.addFaceTab

            Label {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                verticalAlignment: Text.AlignVCenter
                text: root.controller ? root.controller.enrollmentInstruction : "Premi Capture per iniziare"
                color: colors.text
                font.pixelSize: 16
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
        }

        Panel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 370
            color: "#050608"
            border.color: colors.lineStrong
            visible: root.currentTab === root.recognitionTab || root.currentTab === root.addFaceTab

            RowLayout {
                anchors.fill: parent
                anchors.margins: 1
                spacing: root.splitOverlayView ? 8 : 0

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    VideoStream {
                        id: player
                        anchors.fill: parent
                        url: root.controller ? root.controller.videoUrl : ""
                        forceCpuMode: false
                        onFrameImageReady: function(image) {
                            if (root.controller && root.controller.running) {
                                root.controller.submitVideoFrame(image)
                            }
                        }
                    }

                    Image {
                        anchors.fill: parent
                        source: root.controller ? root.controller.frameSource : ""
                        cache: false
                        fillMode: Image.Stretch
                        asynchronous: false
                        visible: !root.splitOverlayView && root.controller && source !== "" && status === Image.Ready
                    }

                    Canvas {
                        id: addFaceGuideOverlay
                        anchors.fill: parent
                        visible: root.currentTab === root.addFaceTab
                        opacity: root.controller && root.controller.enrolling ? 1.0 : 0.92

                        onPaint: {
                            var ctx = getContext("2d")
                            var w = width
                            var h = height
                            ctx.clearRect(0, 0, w, h)

                            var cx = w * 0.5
                            var cy = h * 0.47
                            var radius = Math.min(w, h) * 0.33
                            var guideX = root.controller ? root.controller.enrollmentGuideX : 0
                            var guideY = root.controller ? root.controller.enrollmentGuideY : 0
                            var targetX = cx + guideX * radius * 0.72
                            var targetY = cy + guideY * radius * 0.72

                            ctx.globalCompositeOperation = "source-over"
                            ctx.fillStyle = "rgba(0, 0, 0, 0.80)"
                            ctx.fillRect(0, 0, w, h)

                            ctx.globalCompositeOperation = "destination-out"
                            ctx.beginPath()
                            ctx.arc(cx, cy, radius, 0, Math.PI * 2)
                            ctx.fill()

                            ctx.globalCompositeOperation = "source-over"
                            ctx.lineWidth = 3
                            ctx.strokeStyle = "rgba(244, 247, 248, 0.94)"
                            ctx.beginPath()
                            ctx.arc(cx, cy, radius, 0, Math.PI * 2)
                            ctx.stroke()

                            ctx.lineWidth = 2
                            ctx.strokeStyle = "rgba(101, 214, 198, 0.72)"
                            ctx.beginPath()
                            ctx.arc(cx, cy, radius * 0.72, 0, Math.PI * 2)
                            ctx.stroke()

                            ctx.fillStyle = "rgba(101, 214, 198, 0.98)"
                            ctx.beginPath()
                            ctx.arc(targetX, targetY, 11, 0, Math.PI * 2)
                            ctx.fill()

                            ctx.lineWidth = 2
                            ctx.strokeStyle = "white"
                            ctx.beginPath()
                            ctx.arc(targetX, targetY, 17, 0, Math.PI * 2)
                            ctx.stroke()
                        }

                        onWidthChanged: requestPaint()
                        onHeightChanged: requestPaint()
                        onVisibleChanged: requestPaint()

                        Connections {
                            target: root
                            function onCurrentTabChanged() {
                                addFaceGuideOverlay.requestPaint()
                            }
                        }

                        Connections {
                            target: root.controller
                            function onEnrollmentChanged() {
                                addFaceGuideOverlay.requestPaint()
                            }
                        }
                    }

                    Rectangle {
                        anchors.fill: parent
                        color: "white"
                        opacity: root.controller && root.controller.captureFlash ? 0.55 : 0.0
                        visible: opacity > 0
                    }
                }

                Item {
                    Layout.fillWidth: root.splitOverlayView
                    Layout.fillHeight: true
                    Layout.preferredWidth: root.splitOverlayView ? parent.width / 2 : 0
                    visible: root.splitOverlayView

                    Image {
                        anchors.fill: parent
                        source: root.controller ? root.controller.frameSource : ""
                        cache: false
                        fillMode: Image.Stretch
                        asynchronous: false
                        visible: root.controller && source !== "" && status === Image.Ready
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                text: root.controller ? root.controller.status : "Initializing"
                color: colors.text
                font.pixelSize: 20
                visible: !root.controller || (!root.controller.running && root.controller.status !== "Running")
            }
        }

        Panel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 370
            color: colors.surface
            visible: root.currentTab === root.loadFaceTab

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - 48, 560)
                spacing: 14

                ProgressBar {
                    id: loadProgress
                    indeterminate: root.controller && root.controller.extracting
                    visible: root.controller && root.controller.extracting
                    Layout.fillWidth: true
                    background: Rectangle {
                        implicitHeight: 8
                        radius: 4
                        color: colors.surfaceSoft
                    }
                    contentItem: Item {
                        implicitHeight: 8

                        Rectangle {
                            id: loadProgressFill
                            width: parent.width * 0.36
                            height: parent.height
                            radius: 4
                            color: colors.accent

                            NumberAnimation on x {
                                from: -loadProgressFill.width
                                to: loadProgressFill.parent ? loadProgressFill.parent.width : 0
                                duration: 1100
                                loops: Animation.Infinite
                                running: loadProgress.indeterminate
                            }
                        }
                    }
                }

                Label {
                    text: root.controller ? root.controller.enrollmentStatus : "Ready"
                    color: colors.text
                    font.pixelSize: 20
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }

        Panel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 370
            color: colors.surface
            visible: root.currentTab === root.embeddingsTab

            ListView {
                id: embeddingsList
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                model: root.controller ? root.controller.knownFaces : []
                clip: true

                delegate: Rectangle {
                    id: embeddingRow
                    required property string modelData
                    width: embeddingsList.width
                    height: 56
                    radius: 8
                    color: colors.surfaceRaised
                    border.color: colors.line

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 10
                        spacing: 10

                        Rectangle {
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 28
                            radius: 8
                            color: colors.accentSoft
                            border.color: colors.accent

                            Label {
                                anchors.centerIn: parent
                                text: embeddingRow.modelData.length > 0 ? embeddingRow.modelData.charAt(0).toUpperCase() : "?"
                                color: colors.accent
                                font.pixelSize: 14
                                font.bold: true
                            }
                        }

                        Label {
                            text: embeddingRow.modelData
                            color: colors.text
                            font.pixelSize: 16
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        AppButton {
                            text: "Delete"
                            danger: true
                            compact: true
                            enabled: root.controller !== null
                            onClicked: root.controller.deleteKnownFace(embeddingRow.modelData)
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                text: "No embeddings saved"
                color: colors.muted
                font.pixelSize: 18
                visible: !root.controller || root.controller.knownFaces.length === 0
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            visible: root.currentTab === root.recognitionTab

            Label {
                text: root.controller ? root.controller.status : "Initializing"
                color: root.controller && root.controller.lowLight ? colors.warning : colors.muted
                font.pixelSize: 14
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            StatPill {
                label: "Faces"
                value: root.controller ? root.controller.faceCount : 0
                accentColor: colors.accent
            }

            StatPill {
                label: "FPS"
                value: root.controller ? root.controller.fps.toFixed(1) : "0.0"
                accentColor: colors.violet
            }

            StatPill {
                label: "Light"
                value: root.controller ? root.controller.brightness.toFixed(1) : "0.0"
                accentColor: root.controller && root.controller.lowLight ? colors.warning : colors.accent
            }
        }
    }
}
