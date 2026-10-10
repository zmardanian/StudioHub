import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1540
    height: 960
    minimumWidth: 1120
    minimumHeight: 720
    visible: true
    title: "StudioHub Desktop"
    onClosing: studio.shutdownOwnedProcesses()
    color: palette.window
    palette.window: "#11151b"
    palette.windowText: "#e8edf4"
    palette.base: "#171d25"
    palette.text: "#e8edf4"
    palette.button: "#202a36"
    palette.buttonText: "#e8edf4"
    palette.highlight: "#54c6a3"
    palette.highlightedText: "#07120f"
    palette.placeholderText: "#7e8997"

    Shortcut { sequence: "Ctrl+Return"; onActivated: studio.submitCodexPrompt() }
    Shortcut { sequence: "Ctrl+B"; onActivated: studio.buildVst3() }
    Shortcut { sequence: "Ctrl+T"; onActivated: studio.runDspTests() }
    Shortcut { sequence: "Ctrl+G"; onActivated: studio.runPluginval(true) }
    Shortcut { sequence: "Ctrl+Shift+G"; onActivated: studio.runPluginval(false) }
    Shortcut { sequence: "Ctrl+R"; onActivated: studio.refreshProject() }
    Shortcut { sequence: "Ctrl+M"; onActivated: studio.loadMeasurementReport() }
    Shortcut { sequence: "Ctrl+Q"; onActivated: window.close() }
    Shortcut { sequence: "Escape"; onActivated: studio.stopTask() }

    component ActivityList: ListView {
        clip: true; spacing: 8
        property bool follow: true
        onMovementStarted: follow=false
        onMovementEnded: follow=atYEnd
        onCountChanged: if(follow)Qt.callLater(positionViewAtEnd)
        delegate: Label {
            required property string entryText
            required property string entryKind
            width: ListView.view.width; text: entryKind+"\n"+entryText
            wrapMode: Text.WrapAnywhere; color: entryKind==="user" ? "#72d2b4" : "#c3cfdb"
            font.family: "monospace"; font.pixelSize: 11
            onTextChanged: if(ListView.view && ListView.view.follow)Qt.callLater(ListView.view.positionViewAtEnd)
        }
        ScrollBar.vertical: ScrollBar {}
    }
    component SectionTitle: Label {
        font.pixelSize: 11
        font.weight: Font.DemiBold
        color: "#8290a0"
        font.letterSpacing: 1.1
    }
    component ActionButton: Button {
        property color tint: "#263341"
        background: Rectangle {
            radius: 8
            color: parent.down ? Qt.darker(parent.tint, 1.25) : parent.hovered ? Qt.lighter(parent.tint, 1.15) : parent.tint
            border.color: "#344251"
        }
        contentItem: Text {
            text: parent.text
            color: "#edf4f6"
            font.pixelSize: 12
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    Dialog {
        id: newProjectDialog; title: "New native VST3 project"; modal: true
        anchors.centerIn: parent; width: 700; standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: studio.createProject(projectIdField.text, projectNameField.text, specificationField.text, capabilitySelector.currentValue)
        ColumnLayout { width: parent.width
            TextField { id: projectIdField; Layout.fillWidth: true; placeholderText: "Project ID, e.g. stereo-utility" }
            TextField { id: projectNameField; Layout.fillWidth: true; placeholderText: "Display name" }
            ComboBox {id:capabilitySelector;Layout.fillWidth:true;textRole:"label";valueRole:"value";model:[{label:"No analyzer yet",value:"none"},{label:"Dynamics",value:"dynamics"},{label:"Stereo utility",value:"stereo-utility"}]}
            TextArea { id: specificationField; Layout.fillWidth: true; Layout.preferredHeight: 180; wrapMode: TextEdit.Wrap; placeholderText: "Plugin specification and acceptance requirements" }
            Label { text: "Creates a Git workspace under StudioHub/projects. Submit the prepared Codex task to implement it."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }
    Dialog {
        id: specDialog;title:"Project specification";modal:true;anchors.centerIn: parent;width:700;height:440;standardButtons:Dialog.Save|Dialog.Cancel
        onOpened: specificationEditor.text=studio.specification
        onAccepted: studio.saveSpecification(specificationEditor.text)
        ScrollView {anchors.fill: parent;TextArea {id: specificationEditor;wrapMode:TextEdit.Wrap;selectByMouse:true}}
    }
    Dialog {
        id: toolchainDialog;title:"Toolchain profile";modal:true;anchors.centerIn: parent;width:700;height:400;standardButtons:Dialog.Save|Dialog.Cancel
        onOpened: toolchainEditor.text=studio.toolchainConfiguration
        onAccepted: studio.saveToolchainConfiguration(toolchainEditor.text)
        TextArea {id:toolchainEditor;anchors.fill:parent;wrapMode:TextEdit.Wrap;font.family:"monospace"}
    }
    Dialog {
        id: openProjectDialog; title: "Open StudioHub project"; modal: true
        anchors.centerIn: parent; width: 650; standardButtons: Dialog.Open | Dialog.Cancel
        onAccepted: studio.openProject(manifestField.text)
        TextField { id: manifestField; width: parent.width; placeholderText: "Absolute path to studiohub-project.json" }
    }
    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 250
            Layout.fillHeight: true
            color: "#151a21"
            border.color: "#222b36"
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 18
                RowLayout {
                    spacing: 11
                    Rectangle { width: 34; height: 34; radius: 10; color: "#173c35"
                        Text { anchors.centerIn: parent; text: "S"; color: "#70e0ba"; font.bold: true; font.pixelSize: 18 }
                    }
                    Column {
                        spacing: 2
                        Label { text: "STUDIOHUB"; font.pixelSize: 13; font.bold: true; color: "#f2f5f8"; font.letterSpacing: 1 }
                        Label { text: "DESKTOP  ·  0.1"; font.pixelSize: 9; color: "#728091"; font.letterSpacing: 1 }
                    }
                }
                Rectangle { Layout.fillWidth: true; height: 1; color: "#252d37" }
                SectionTitle { text: "PROJECTS" }
                Rectangle {
                    Layout.fillWidth: true
                    height: 76
                    radius: 10
                    color: "#1b2a2c"
                    border.color: "#367b6b"
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10
                        Rectangle { width: 5; Layout.fillHeight: true; radius: 3; color: "#60d7b0" }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            Label { text: studio.projectName; font.pixelSize: 13; font.bold: true; color: "#e8f5f1" }
                            Label { text: "VST3  ·  Native C++20"; font.pixelSize: 10; color: "#8ba99f" }
                        }
                        Label { text: "●"; color: "#59d3a9"; font.pixelSize: 10 }
                    }
                }
                ComboBox { Layout.fillWidth: true; model: studio.projects; currentIndex: studio.selectedProjectIndex; onActivated: studio.selectProject(currentIndex) }
                ActionButton { text: "New project…"; Layout.fillWidth: true; onClicked: newProjectDialog.open() }
                ActionButton { text: "Open project…"; Layout.fillWidth: true; onClicked: openProjectDialog.open() }
                Item { Layout.fillHeight: true }
                Rectangle { Layout.fillWidth: true; height: 1; color: "#252d37" }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 7
                    SectionTitle { text: "CODEX RUNTIME" }
                    RowLayout {
                        spacing: 8
                        Rectangle { width: 8; height: 8; radius: 4; color: studio.codexState.indexOf("Connected") >= 0 || studio.codexState.indexOf("Turn completed") === 0 ? "#55d5a8" : studio.codexState.indexOf("Turn failed") === 0 || studio.codexState.indexOf("Protocol error") === 0 ? "#e26d72" : "#dda858" }
                        Label { text: studio.codexState; color: "#c6d0db"; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight }
                    }
                    Label { text: "Working directory follows the selected project"; wrapMode: Text.WordWrap; color: "#758394"; font.pixelSize: 10; Layout.fillWidth: true }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            Rectangle {
                Layout.fillWidth: true
                height: 66
                color: "#171d25"
                border.color: "#252d37"
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    Label { text: "ENGINEERING WORKSPACE"; color: "#8492a1"; font.pixelSize: 10; font.letterSpacing: 1.2 }
                    Item { Layout.fillWidth: true }
                    Label { text: studio.activeTask; color: studio.activeTask === "Idle" ? "#8d9aaa" : "#75ddbb"; font.pixelSize: 11 }
                    ActionButton { text: "Stop  Esc"; tint: "#39262b"; onClicked: studio.stopTask() }
                }
            }
            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: width
                contentHeight: overview.implicitHeight + 48
                ColumnLayout {
                    id: overview
                    x: 24; y: 22
                    width: parent.width - 48
                    spacing: 18
                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            Label { text: studio.projectName; font.pixelSize: 28; font.weight: Font.DemiBold; color: "#f0f4f7" }
                            Label { text: studio.description; color: "#91a0af"; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        }
                        Rectangle { radius: 20; color: "#2e2920"; border.color: "#66552f"; implicitWidth: statusText.implicitWidth + 20; implicitHeight: 28
                            Label { id: statusText; anchors.centerIn: parent; text: studio.validationState; color: "#edc983"; font.pixelSize: 10 }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        radius: 12
                        color: "#171d25"
                        border.color: "#29323e"
                        implicitHeight: projectCard.implicitHeight + 30
                        ColumnLayout {
                            id: projectCard
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10
                            SectionTitle { text: "SOURCE & ARTIFACT" }
                            Label { text: studio.projectPath; color: "#c4ced9"; font.family: "monospace"; font.pixelSize: 10; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: "REVISION"; color: "#7f8c9b"; font.pixelSize: 9; font.letterSpacing: 1 }
                                Label { text: studio.revision; color: "#c5d2df"; font.pixelSize: 11; font.family: "monospace" }
                                Item { Layout.fillWidth: true }
                                Label { text: "BUILD"; color: "#7f8c9b"; font.pixelSize: 9; font.letterSpacing: 1 }
                                Label { text: studio.buildState; color: studio.buildState.indexOf("PASS") >= 0 ? "#65d6ac" : "#e4bd71"; font.pixelSize: 11 }
                            }
                            Rectangle { Layout.fillWidth: true; height: 1; color: "#29323e" }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: "Artifact"; color: "#8290a0"; font.pixelSize: 10 }
                                Label { text: studio.artifactPath; color: "#b8c4d0"; font.family: "monospace"; font.pixelSize: 9; Layout.fillWidth: true; elide: Text.ElideMiddle }
                            }
                            Label { text: "SHA-256  " + studio.artifactHash; color: "#8290a0"; font.family: "monospace"; font.pixelSize: 9; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere }
                            Label { text: "Latest evidence  " + studio.latestEvidence; color: "#8290a0"; font.pixelSize: 9; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Repeater {
                            model: [
                                {title:"BUILD", value:studio.buildState, note:"Isolated Release bundle"},
                                {title:"DSP TESTS", value:studio.dspState, note:"Manifest-selected DSP and integration tests"},
                                {title:"GUI LIFECYCLE", value:studio.guiLifecycleState, note:"pluginval strictness 5 · exact module hash"}
                            ]
                            delegate: Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 91
                                radius: 10
                                color: "#171d25"
                                border.color: "#29323e"
                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 5
                                    SectionTitle { text: modelData.title }
                                    Label { text: modelData.value; color: modelData.value.indexOf("PASS") >= 0 ? "#65d6ac" : modelData.value.indexOf("FAIL") >= 0 ? "#f08a83" : "#edc983"; font.pixelSize: 13; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                                    Label { text: modelData.note; color: "#7f8c9b"; font.pixelSize: 9; Layout.fillWidth: true; elide: Text.ElideRight }
                                }
                            }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        radius: 12
                        color: "#171d25"
                        border.color: "#29323e"
                        implicitHeight: actions.implicitHeight + 28
                        ColumnLayout {
                            id: actions
                            anchors.fill: parent
                            anchors.margins: 15
                            spacing: 11
                            RowLayout {
                                Layout.fillWidth: true
                                SectionTitle { text: "VALIDATION & MEASUREMENTS" }
                                Item { Layout.fillWidth: true }
                                Label { text: "Exact artifact results only"; color: "#758394"; font.pixelSize: 9 }
                            }
                            Flow {
                                Layout.fillWidth: true
                                spacing: 8
                                ActionButton { text: "Verify project"; tint:"#1e423b";onClicked:studio.verifyProject() }
                                ActionButton { text: "Build VST3"; onClicked: studio.buildVst3() }
                                ActionButton { text: "Run DSP tests"; onClicked: studio.runDspTests() }
                                ActionButton { text: "pluginval · GUI"; tint: "#39262b"; onClicked: studio.runPluginval(true) }
                                ActionButton { text: "pluginval · non-GUI"; onClicked: studio.runPluginval(false) }
                                ActionButton { text: "Steinberg"; onClicked: studio.runSdkValidator() }
                                ActionButton { text: "Metadata"; onClicked: studio.runMetadataValidator() }
                                ActionButton { text: "Measure"; onClicked: studio.runMeasurements() }
                                ActionButton { text:"Specification";onClicked:specDialog.open() }
                                ActionButton { text:"Toolchain";onClicked:toolchainDialog.open() }
                                ActionButton { text: "Load measurements"; onClicked: studio.loadMeasurementReport() }
                                ActionButton { text: "Open evidence folder"; onClicked: studio.openEvidenceFolder() }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        ActionButton { text: "Inspect artifact"; onClicked: studio.openArtifact() }
                        ActionButton { text: "Prepare repair"; onClicked: studio.prepareRepair() }
                        Label { text: "SDK " + studio.sdkState + " · Metadata " + studio.metadataState + " · Measurements " + studio.measurementState; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#a5b4c5"; font.pixelSize: 10 }
                    }
                    ScrollView {
                        Layout.fillWidth: true; Layout.preferredHeight: studio.history.length ? 100 : 0
                        visible: studio.history.length > 0; clip: true
                        TextArea { text: studio.history; readOnly: true; wrapMode: TextEdit.Wrap; font.pixelSize: 9; color: "#8495a8" }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 8
                        Label {text:"Latest successful evidence: "+studio.latestSuccessfulEvidence;Layout.fillWidth:true;wrapMode:Text.WordWrap;color:"#8495a8"}
                        Label {text:studio.workflowState;Layout.fillWidth:true;wrapMode:Text.WordWrap;color:studio.workflowState.startsWith("Verification completed")?"#72d2b4":"#e5b35d"}
                        Label {text:studio.recoveryState;visible:text.length>0;Layout.fillWidth:true;wrapMode:Text.WordWrap;color:"#edc983"}
                        ActionButton {text:"Retry interrupted job";visible:studio.recoveryState.length>0;onClicked:studio.retryInterruptedJob()}
                        ComboBox {Layout.fillWidth:true;model:studio.evidenceEntries;textRole:"label";onActivated:studio.inspectEvidence(currentIndex);displayText:currentText ? "Inspect evidence: "+currentText : "No evidence"}
                        SectionTitle {text:"LIVE BUILD & EVIDENCE LOG"}
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 220
                            radius: 10
                            color: "#0d1116"
                            border.color: "#252d37"
                            ScrollView {
                                anchors.fill: parent; anchors.margins: 10; clip: true
                                ActivityList { width: parent.width; height: parent.height; model: studio.buildLogModel }

                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 390
            Layout.fillHeight: true
            color: "#151a21"
            border.color: "#252d37"
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 17
                spacing: 12
                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        spacing: 3
                        Label { text: "Codex assistant"; font.pixelSize: 15; font.bold: true; color: "#eff4f8" }
                        Label { text: "App Server · selected project cwd"; color: "#7e8b99"; font.pixelSize: 10 }
                        RowLayout {
                            ActionButton { text: "Connect"; onClicked: studio.connectCodex() }
                            ComboBox { displayText: currentText || studio.selectedModel; model: studio.models; currentIndex: studio.models.indexOf(studio.selectedModel); onActivated: studio.selectModel(currentIndex); Layout.preferredWidth: 160 }
                            ComboBox { displayText: currentText || "default"; model: studio.reasoningEfforts; currentIndex: studio.reasoningEfforts.indexOf(studio.selectedEffort); onActivated: studio.selectEffort(currentIndex); Layout.preferredWidth: 85 }
                        }
                        Label { text: studio.authenticationState + " · " + studio.effectiveModel; color: "#8292a1"; font.pixelSize: 9; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                        Label { text: studio.codexPermissionState; color: "#8292a1"; font.pixelSize: 9; elide: Text.ElideRight; Layout.fillWidth: true }
                    }
                    Item { Layout.fillWidth: true }
                    Rectangle { width: 8; height: 8; radius: 4; color: studio.codexState.indexOf("Connected") >= 0 || studio.codexState.indexOf("Turn completed") === 0 ? "#55d5a8" : studio.codexState.indexOf("Turn failed") === 0 || studio.codexState.indexOf("Protocol error") === 0 ? "#e26d72" : "#dda858" }
                }
                Rectangle { Layout.fillWidth: true; height: 1; color: "#252d37" }
                RowLayout {
                    Layout.fillWidth: true
                    ActionButton { text: "Run required read-only review"; Layout.fillWidth: true; tint: "#1e423b"; onClicked: studio.runReadOnlyCodexTurn() }
                }
                TextArea {
                    id: prompt
                    Layout.fillWidth: true
                    Layout.preferredHeight: 92
                    placeholderText: "Ask Codex to develop the selected project…"
                    text: studio.prompt
                    onTextChanged: if (studio.prompt !== text) studio.prompt = text
                    wrapMode: TextEdit.Wrap
                    color: "#e5edf4"
                    font.pixelSize: 11
                    background: Rectangle { radius: 8; color: "#0d1116"; border.color: prompt.activeFocus ? "#498d78" : "#303b48" }
                }
                RowLayout {
                    Layout.fillWidth: true
                    ActionButton { text: "Send  Ctrl+↵"; Layout.fillWidth: true; tint: "#1e423b"; onClicked: studio.submitCodexPrompt() }
                    ActionButton { text: "Stop"; onClicked: studio.stopTask() }
                }
                Label { text: "Tool approvals: automatically accepted · your setting"; color: "#9bbaa9"; font.pixelSize: 10; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                TabBar { id: assistantTabs; Layout.fillWidth: true; TabButton {text:"Conversation"} TabButton {text:"Tools"} }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 10
                    color: "#0d1116"
                    border.color: "#252d37"
                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 10
                        clip: true
                        ActivityList { width: parent.width; height: parent.height; model: assistantTabs.currentIndex===0 ? studio.conversationModel : studio.operationModel }

                    }
                }
                Label { text: "Codex uses the installed runtime and its configured sign-in. Known tool requests are automatically accepted; unsupported requests are rejected."; color: "#758394"; font.pixelSize: 9; wrapMode: Text.Wrap; Layout.fillWidth: true }
            }
        }
    }
}
