// Test build (CONFIG+=lav_test) logic:
// - the allowlist (app/backend/testguard), the logic behind NvHTTP::isAllowedTestAddress:
//   loopback only, ports 40000-44999 only;
// - the testbench command channel (app/testctl): protocol, key names, the reset guard, and
//   what `tree` / `click` / `key` / `text` / `wheel` do on a small QML scene (offscreen).

#include "../../app/backend/testguard.h"
#include "../../app/testctl/testprotocol.h"
#include "../../app/testctl/uiprobe.h"

#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSet>

#include <cstdio>
#include <memory>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#ifdef Q_OS_WIN32
#include <Windows.h>
#endif

namespace {
int failures = 0;

void check(bool condition, const char* description)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++failures;
    }
}

QJsonObject response(const QByteArray& line)
{
    return QJsonDocument::fromJson(line).object();
}

void allowlistTests()
{
    check(TestGuard::isAllowedAddress("127.0.0.1", 40989), "loopbackFirstTestHost");
    check(!TestGuard::isAllowedAddress("127.0.0.1", 47989), "defaultPortRefused");
    check(!TestGuard::isAllowedAddress("127.0.0.1", 45989), "realHostOfThisPcRefused");
    check(!TestGuard::isAllowedAddress("192.168.1.231", 40989), "lanAddressRefused");
    check(TestGuard::isAllowedAddress("localhost", 41984), "localhostAllowed");
    check(!TestGuard::isAllowedAddress("127.0.0.1", 39999), "belowRangeRefused");
    check(!TestGuard::isAllowedAddress("127.0.0.1", 45000), "aboveRangeRefused");
    check(TestGuard::isAllowedAddress("127.0.0.1", 40000), "lowerBoundAllowed");
    check(TestGuard::isAllowedAddress("127.0.0.1", 44999), "upperBoundAllowed");
    check(TestGuard::isAllowedAddress("::1", 42000), "ipv6LoopbackAllowed");
    check(TestGuard::isAllowedAddress("[::1]", 42000), "bracketedIpv6Allowed");
    check(!TestGuard::isAllowedAddress("", 42000), "emptyHostRefused");
    check(!TestGuard::isAllowedAddress("10.0.0.1", 42000), "otherPrivateRefused");
}

void protocolTests()
{
    using namespace TestProtocol;

    Dispatcher dispatcher;
    dispatcher.add("echo", [](const QJsonObject& args, QJsonObject& result, QString&) {
        result["said"] = args["say"];
        result["id"] = 999;  // must not override the protocol's own id
        return true;
    });
    dispatcher.add("fail", [](const QJsonObject&, QJsonObject&, QString& error) {
        error = "no luck";
        return false;
    });

    QByteArray line = dispatcher.handleLine("{this is not json");
    QJsonObject r = response(line);
    check(line.endsWith('\n'), "responseEndsWithNewline");
    check(r.contains("id") && r["id"].isNull(), "malformedLineHasNullId");
    check(r["ok"] == false && !r["error"].toString().isEmpty(), "malformedLineIsAnError");

    r = response(dispatcher.handleLine("[1, 2]"));
    check(r["id"].isNull() && r["ok"] == false, "nonObjectIsAnError");

    r = response(dispatcher.handleLine(R"({"id": 7})"));
    check(r["id"] == 7 && r["ok"] == false, "missingCmdIsAnErrorWithItsId");

    r = response(dispatcher.handleLine(R"({"id": 3, "cmd": "nope"})"));
    check(r["id"] == 3 && r["ok"] == false, "unknownCommandIsAnError");
    check(r["error"].toString().contains("nope"), "unknownCommandErrorNamesIt");

    r = response(dispatcher.handleLine(R"({"id": 4, "cmd": "echo", "say": "salut"})"));
    check(r["id"] == 4 && r["ok"] == true && r["said"] == "salut", "commandResultIsMerged");

    // No command runs inside another one (a nested event loop of the first)
    QJsonObject inner;
    dispatcher.add("nest", [&dispatcher, &inner](const QJsonObject&, QJsonObject& result, QString&) {
        check(dispatcher.isRunning(), "dispatcherRunningDuringCommand");
        inner = response(dispatcher.handleLine(R"({"id": 6, "cmd": "echo", "say": "x"})"));
        result["done"] = true;
        return true;
    });
    r = response(dispatcher.handleLine(R"({"id": 5, "cmd": "nest"})"));
    check(r["ok"] == true && inner["id"] == 6 && inner["ok"] == false, "nestedCommandRefused");
    check(!dispatcher.isRunning(), "dispatcherIdleAfterCommand");

    r = response(dispatcher.handleLine(R"({"id": "abc", "cmd": "fail"})"));
    check(r["id"] == "abc" && r["ok"] == false && r["error"] == "no luck", "handlerErrorIsReported");

    Request request;
    QString error;
    check(parseRequest(R"({"cmd":"tree","textOnly":true})", request, error) &&
          request.cmd == "tree" && request.id.isNull() && request.args["textOnly"] == true,
          "parseRequestKeepsArguments");
    check(!parseRequest(R"({"cmd": 5})", request, error), "nonStringCmdRefused");

    // Key names: Qt key, USB HID usage (= SDL scancode), SDL keycode on a US layout
    const KeySpec* key = findKey("enter");
    check(key != nullptr && key->scancode == 40 && key->qtKey == Qt::Key_Return && key->sdlKey == '\r',
          "enterKey");
    check(findKey("Return") != nullptr && findKey("Return")->scancode == 40, "returnAlias");
    key = findKey("A");
    check(key != nullptr && key->scancode == 4 && key->qtKey == Qt::Key_A && key->sdlKey == 'a', "letterA");
    key = findKey("z");
    check(key != nullptr && key->scancode == 29 && key->sdlKey == 'z', "letterZLowercaseName");
    key = findKey("0");
    check(key != nullptr && key->scancode == 39 && key->qtKey == Qt::Key_0 && key->sdlKey == '0', "digit0");
    key = findKey("1");
    check(key != nullptr && key->scancode == 30 && key->sdlKey == '1', "digit1");
    key = findKey("F1");
    check(key != nullptr && key->scancode == 58 && key->qtKey == Qt::Key_F1 && key->sdlKey == (58 | (1 << 30)),
          "functionKey");
    key = findKey("F12");
    check(key != nullptr && key->scancode == 69, "f12");
    key = findKey("Escape");
    check(key != nullptr && key->scancode == 41 && key->qtKey == Qt::Key_Escape && key->sdlKey == 27, "escape");
    check(findKey("Esc") != nullptr && findKey("Esc")->scancode == 41 && findKey("Esc")->qtKey == Qt::Key_Escape, "escAlias");
    key = findKey("Grave");
    check(key != nullptr && key->scancode == 53 && key->sdlKey == '`' && key->qtKey == Qt::Key_QuoteLeft, "grave");
    key = findKey("Left");
    check(key != nullptr && key->scancode == 80 && key->qtKey == Qt::Key_Left && key->sdlKey == (80 | (1 << 30)),
          "leftArrow");
    key = findKey("Delete");
    check(key != nullptr && key->scancode == 76 && key->sdlKey == 127, "delete");
    key = findKey("Win");
    check(key != nullptr && findKey("Meta")->scancode == 227 && key->scancode == 227 && key->qtKey == Qt::Key_Meta,
          "winIsMeta");
    key = findKey("RCtrl");
    check(key != nullptr && key->scancode == 228 && key->qtKey == Qt::Key_Control, "rightCtrl");
    check(findKey("Nope") == nullptr && findKey("") == nullptr, "unknownKeyName");

    QSet<QString> names;
    bool allFindable = true;
    for (const KeySpec& spec : keyTable()) {
        allFindable = allFindable && findKey(spec.name) != nullptr && findKey(spec.name)->scancode == spec.scancode;
        names.insert(QString(spec.name).toLower());
    }
    check(allFindable && names.size() == keyTable().size(), "keyTableNamesUniqueAndFindable");

    KeySpec charKey;
    bool shift = true;
    check(keyForChar('a', charKey, shift) && charKey.scancode == 4 && !shift, "charLowercase");
    check(keyForChar('A', charKey, shift) && charKey.scancode == 4 && shift, "charUppercaseUsesShift");
    check(keyForChar('!', charKey, shift) && charKey.scancode == 30 && shift, "charBangIsShift1");
    check(keyForChar(' ', charKey, shift) && charKey.scancode == 44 && !shift, "charSpace");
    check(keyForChar('\n', charKey, shift) && charKey.scancode == 40, "charNewlineIsEnter");
    check(keyForChar('_', charKey, shift) && charKey.scancode == 45 && shift, "charUnderscore");
    check(keyForChar('/', charKey, shift) && charKey.scancode == 56 && !shift, "charSlash");
    check(!keyForChar(QChar(0x00E9), charKey, shift), "charAccentRefused");

    int mask = -1;
    check(parseModifiers(QJsonValue("Ctrl+Shift"), mask, error) && mask == (ModCtrl | ModShift), "modifiersString");
    check(parseModifiers(QJsonArray{"alt", "win"}, mask, error) && mask == (ModAlt | ModMeta), "modifiersArray");
    check(parseModifiers(QJsonValue(), mask, error) && mask == 0, "modifiersAbsent");
    check(!parseModifiers(QJsonValue("Hyper"), mask, error), "modifiersUnknownRefused");

    // reset may only ever wipe the test build's own organisation
    check(isWipeableOrganization("LavArtemis Test"), "testOrgWipeable");
    for (const char* org : {"LavArtemis", "LavArtemis Dev", "lavartemis test", "LavArtemis Test ", "",
                            "LavArtemis Test/..", "Moonlight Game Streaming Project"}) {
        if (isWipeableOrganization(org)) {
            std::fprintf(stderr, "FAIL: orgRefused '%s'\n", org);
            ++failures;
        }
    }
    check(organizationFolder("C:/Users/x/AppData/Local/LavArtemis Test/LavArtemis", "LavArtemis Test") ==
          "C:/Users/x/AppData/Local/LavArtemis Test", "orgFolderFromAppFolder");
    check(organizationFolder("C:\\Users\\x\\AppData\\Local\\LavArtemis Test\\LavArtemis\\cache", "LavArtemis Test") ==
          "C:/Users/x/AppData/Local/LavArtemis Test", "orgFolderFromCacheFolder");
    check(organizationFolder("C:/Users/x/AppData/Local/LavArtemis Dev/LavArtemis", "LavArtemis Test").isEmpty(),
          "devFolderRefused");
    check(organizationFolder("C:/Users/x/AppData/Local/LavArtemis/LavArtemis", "LavArtemis").isEmpty(),
          "stableOrgRefused");
    check(organizationFolder("C:/Users/x/AppData/Local/LavArtemis Dev/LavArtemis", "LavArtemis Dev").isEmpty(),
          "devOrgRefused");
    check(organizationFolder("C:/LavArtemis Test/a/b/c", "LavArtemis Test").isEmpty(), "tooDeepRefused");
    check(organizationFolder("LavArtemis Test/LavArtemis", "LavArtemis Test").isEmpty(), "relativeRefused");
    check(registryKeyFor("LavArtemis Test") == "Software\\LavArtemis Test", "registryKeyForTestOrg");
    check(registryKeyFor("LavArtemis").isEmpty() && registryKeyFor("LavArtemis Dev").isEmpty(),
          "registryKeyRefusedForOthers");

    // screenshot paths: absolute and local only
    check(isLocalAbsolutePath("C:/Users/x/shot.png"), "localAbsolutePathAccepted");
    check(!isLocalAbsolutePath("shot.png"), "relativePathRefused");
    check(!isLocalAbsolutePath("//server/share/shot.png"), "uncSlashPathRefused");
    check(!isLocalAbsolutePath("\\\\server\\share\\shot.png"), "uncBackslashPathRefused");
    check(!isLocalAbsolutePath("\\\\?\\C:\\shot.png"), "devicePathRefused");

    // Request lines: at most 1 MiB, whether the line comes in one read or in pieces
    {
        LineBuffer buffer;
        QList<LineBuffer::Line> lines = buffer.feed("{\"cmd\":\"a\"}\r\n\n  \n{\"cmd\":\"b\"}\n{\"cm");
        check(lines.size() == 2 && lines[0].text == "{\"cmd\":\"a\"}" && !lines[0].tooLong &&
              lines[1].text == "{\"cmd\":\"b\"}", "linesSplitTrimmedEmptySkipped");
        lines = buffer.feed("d\":\"c\"}\n");
        check(lines.size() == 1 && lines[0].text == "{\"cmd\":\"c\"}", "linePartialThenCompleted");

        const QByteArray big(LineBuffer::kMaxLine + 100, 'x');
        lines = buffer.feed(big + "\n{\"cmd\":\"d\"}\n");
        check(lines.size() == 2 && lines[0].tooLong && lines[1].text == "{\"cmd\":\"d\"}",
              "longLineInOneReadRefusedOnce");

        lines = buffer.feed(big);
        bool oneError = lines.size() == 1 && lines[0].tooLong;
        lines = buffer.feed(big);
        oneError = oneError && lines.isEmpty();
        lines = buffer.feed("tail\n{\"cmd\":\"e\"}\n");
        check(oneError && lines.size() == 1 && lines[0].text == "{\"cmd\":\"e\"}", "longLineInPiecesRefusedOnce");

        const QByteArray exact(LineBuffer::kMaxLine, 'y');
        lines = buffer.feed(exact + "\n");
        check(lines.size() == 1 && !lines[0].tooLong, "lineAtTheLimitAccepted");
    }
}

// reset deletes a tree without ever following a junction or symbolic link out of it
void removeTreeTests()
{
#ifdef Q_OS_WIN32
    QTemporaryDir tree;
    QTemporaryDir victim;
    QFile keep(victim.filePath("keep.txt"));
    check(keep.open(QIODevice::WriteOnly), "victimFileCreated");
    keep.write("keep");
    keep.close();
    QDir(tree.path()).mkpath("sub/deeper");
    QFile inside(tree.filePath("sub/deeper/file.txt"));
    check(inside.open(QIODevice::WriteOnly), "treeFileCreated");
    inside.close();
    const QString link = QDir::toNativeSeparators(tree.filePath("sub/link"));
    const int made = QProcess::execute("cmd", {"/c", "mklink", "/J", link, QDir::toNativeSeparators(victim.path())});
    check(made == 0 && QFileInfo(tree.filePath("sub/link")).isJunction(), "junctionCreated");

    // A junction as the root is refused, and its target untouched
    check(!TestProtocol::removeTreeNoFollow(tree.filePath("sub/link")), "junctionRootRefused");
    check(QFile::exists(victim.filePath("keep.txt")), "junctionRootTargetKept");

    check(TestProtocol::removeTreeNoFollow(tree.path()), "treeRemoved");
    check(!QFileInfo::exists(tree.path()), "treeGone");
    check(QFile::exists(victim.filePath("keep.txt")), "junctionTargetKept");
#endif
}

const char* kScene = R"QML(
import QtQuick
import QtQuick.Window
Window {
    id: root
    width: 400; height: 360
    property int taps: 0
    flags: Qt.Window | Qt.WindowDoesNotAcceptFocus
    property int clicks: 0
    property int rightClicks: 0
    property int wheels: 0
    property alias typed: input.text
    MouseArea {
        objectName: "bonjourArea"
        x: 40; y: 40; width: 160; height: 40
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: (mouse) => { if (mouse.button === Qt.RightButton) root.rightClicks++; else root.clicks++ }
        onWheel: (wheel) => { root.wheels += wheel.angleDelta.y }
        Text { anchors.centerIn: parent; text: "Bonjour" }
    }
    Text { text: "Hidden"; visible: false; x: 10; y: 200 }
    Text { text: "Transparent"; opacity: 0; x: 10; y: 220 }
    Item { opacity: 0; Text { text: "TransparentParent"; x: 10; y: 240 } }
    Text { text: "Outside"; x: 1000; y: 10 }
    Item { x: 250; y: 40; width: 100; height: 30; clip: true
        Text { text: "Clipped"; y: 50 }
    }
    property int escapes: 0
    FocusScope {
        x: 40; y: 120; width: 200; height: 30
        focus: true
        Keys.onEscapePressed: root.escapes++
        TextInput { id: input; objectName: "input"; anchors.fill: parent; focus: true }
    }
    Text { text: "Bonjour"; x: 40; y: 260 }
    Text { text: "Covered"; x: 250; y: 200 }
    MouseArea { objectName: "cover"; x: 240; y: 190; width: 120; height: 40 }
    Rectangle {
        objectName: "tapCard"; x: 250; y: 250; width: 120; height: 40
        TapHandler { onTapped: root.taps++ }
        Text { anchors.centerIn: parent; text: "TapCard" }
    }
    Text { text: "UnderTap"; x: 20; y: 310 }
    Item { objectName: "tapCover"; x: 10; y: 300; width: 100; height: 40; TapHandler {} }
    Text { text: "UnderHover"; x: 150; y: 310 }
    Item { objectName: "hoverOnly"; x: 140; y: 300; width: 90; height: 40; HoverHandler {} }
}
)QML";

void uiTests()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(kScene, QUrl());
    std::unique_ptr<QObject> root(component.create());
    auto* window = qobject_cast<QQuickWindow*>(root.get());
    check(window != nullptr, "sceneLoads");
    if (window == nullptr) {
        std::fprintf(stderr, "%s\n", qPrintable(component.errorString()));
        return;
    }
    window->show();
    QCoreApplication::processEvents();

    const QList<QQuickWindow*> windows = UiProbe::windows(window);
    check(windows.size() == 1 && windows.first() == window, "windowsListsTheScene");

    QList<UiProbe::Item> items = UiProbe::collect(windows);
    auto has = [&items](const QString& text) {
        for (const UiProbe::Item& item : items) {
            if (item.text == text) return true;
        }
        return false;
    };
    check(has("Bonjour"), "treeHasBonjour");
    check(!has("Hidden"), "treeSkipsInvisible");
    check(!has("Transparent") && !has("TransparentParent"), "treeSkipsTransparent");
    check(!has("Outside"), "treeSkipsOutsideWindow");
    check(!has("Clipped"), "treeSkipsClippedAway");

    UiProbe::Item bonjour;
    QString error;
    check(UiProbe::find(items, QJsonObject{{"text", "Bonjour"}}, bonjour, error), "findBonjour");
    QJsonObject json = UiProbe::toJson(bonjour);
    check(json["type"] == "Text" && json["text"] == "Bonjour" && json["window"] == 0, "bonjourJson");
    const QRectF expected(40 + (160 - bonjour.rect.width()) / 2, 40 + (40 - bonjour.rect.height()) / 2,
                          bonjour.rect.width(), bonjour.rect.height());
    check(qAbs(bonjour.rect.x() - expected.x()) < 1 && qAbs(bonjour.rect.y() - expected.y()) < 1,
          "bonjourRectInWindowCoordinates");
    check(json.contains("x") && json.contains("w") && json.contains("focus") && json.contains("enabled") &&
          json.contains("objectName"), "itemJsonFields");

    UiProbe::Item last;
    check(UiProbe::find(items, QJsonObject{{"text", "Bonjour"}, {"index", -1}}, last, error) &&
          last.rect.y() > 250, "findIndexFromEnd");
    UiProbe::Item partial;
    check(UiProbe::find(items, QJsonObject{{"text", "onjou"}, {"contains", true}}, partial, error),
          "findContains");
    check(!UiProbe::find(items, QJsonObject{{"text", "onjou"}}, partial, error) && !error.isEmpty(),
          "findIsExactByDefault");
    UiProbe::Item area;
    check(UiProbe::find(items, QJsonObject{{"objectName", "bonjourArea"}}, area, error) &&
          UiProbe::typeName(area.item) == "MouseArea", "findByObjectName");

    check(UiProbe::coveringItem(windows, bonjour) == nullptr, "bonjourNotCovered");
    UiProbe::Item covered;
    check(UiProbe::find(items, QJsonObject{{"text", "Covered"}}, covered, error), "findCovered");
    QQuickItem* cover = UiProbe::coveringItem(windows, covered);
    check(cover != nullptr && cover->objectName() == "cover", "coveredTextReportsItsCover");

    // Pointer handlers take clicks too (TapHandler cards); a HoverHandler does not
    UiProbe::Item underTap, underHover, tapCard;
    check(UiProbe::find(items, QJsonObject{{"text", "UnderTap"}}, underTap, error) &&
          UiProbe::find(items, QJsonObject{{"text", "UnderHover"}}, underHover, error) &&
          UiProbe::find(items, QJsonObject{{"text", "TapCard"}}, tapCard, error), "findHandlerItems");
    cover = UiProbe::coveringItem(windows, underTap);
    check(cover != nullptr && cover->objectName() == "tapCover", "coveredByTapHandler");
    check(UiProbe::coveringItem(windows, underHover) == nullptr, "hoverHandlerDoesNotCover");
    check(UiProbe::coveringItem(windows, tapCard) == nullptr, "tapCardNotCovered");
    UiProbe::click(window, tapCard.rect.center(), Qt::LeftButton);
    check(root->property("taps").toInt() == 1, "clickTapsTapHandler");

#ifdef Q_OS_WIN32
    // Synthetic input on the clock of real Windows input (GetMessageTime / GetTickCount)
    const qint64 drift = qint64(UiProbe::inputTimestamp()) - qint64(GetTickCount());
    check(drift > -1000 && drift < 1000, "inputTimestampOnTheWindowsClock");
#endif

    // The click must not pump the event loop: a command never runs inside another one
    bool queuedRan = false;
    QMetaObject::invokeMethod(QCoreApplication::instance(), [&queuedRan]() { queuedRan = true; }, Qt::QueuedConnection);
    UiProbe::click(window, bonjour.rect.center(), Qt::LeftButton);
    check(!queuedRan, "clickDoesNotRunTheEventLoop");
    QCoreApplication::processEvents();
    check(queuedRan, "queuedCallRunsAfterwards");
    check(root->property("clicks").toInt() == 1, "clickOnTextTriggersParentMouseArea");
    UiProbe::click(window, area.rect.center(), Qt::RightButton);
    check(root->property("rightClicks").toInt() == 1, "rightClick");

    UiProbe::wheel(window, area.rect.center(), QPoint(0, 120));
    check(root->property("wheels").toInt() == 120, "wheelReachesItemUnderPoint");

    // Another window takes the focus: the scene's window has no active focus item any more,
    // like the test client's windows, which never get the OS focus.
    QQuickWindow thief;
    thief.resize(50, 50);
    thief.show();
    thief.requestActivate();
    QCoreApplication::processEvents();
    check(window->activeFocusItem() == nullptr, "sceneWindowLostActiveFocus");
    {
        // tree.focus: the focus chain (where keys go), not the active focus nobody has
        bool inputFocus = false;
        bool bonjourFocus = true;
        for (const UiProbe::Item& item : UiProbe::collect(windows)) {
            if (item.objectName == "input") inputFocus = item.focus;
            if (item.text == "Bonjour") bonjourFocus = bonjourFocus && item.focus;
        }
        check(inputFocus, "treeFocusFollowsTheFocusChain");
        check(!bonjourFocus, "treeFocusFalseOutsideTheChain");
    }
    UiProbe::typeText(window, "abc");
    check(root->property("typed").toString() == "abc", "textTypedIntoFocusedInput");
    UiProbe::key(window, Qt::Key_Backspace, Qt::NoModifier, UiProbe::KeyState::Click);
    check(root->property("typed").toString() == "ab", "keyBackspace");
    UiProbe::key(window, Qt::Key_Escape, Qt::NoModifier, UiProbe::KeyState::Click);
    check(root->property("escapes").toInt() == 1, "unhandledKeyReachesParentKeysHandler");

    items = UiProbe::collect(windows, true);
    bool onlyNamed = !items.isEmpty();
    for (const UiProbe::Item& item : items) {
        onlyNamed = onlyNamed && (!item.text.isEmpty() || !item.objectName.isEmpty());
    }
    check(onlyNamed, "textOnlyKeepsNamedItems");
}

// state.page: the page on top of the StackView, by type; its objectName is a title
const char* kStackScene = R"QML(
import QtQuick
import QtQuick.Controls
ApplicationWindow {
    width: 300; height: 200
    flags: Qt.Window | Qt.WindowDoesNotAcceptFocus
    StackView {
        anchors.fill: parent
        initialItem: Rectangle { objectName: "Titre de la page" }
    }
}
)QML";

// The Qt Quick Controls overlay covers the page only while a modal popup is open
const char* kPopupScene = R"QML(
import QtQuick
import QtQuick.Controls
ApplicationWindow {
    width: 400; height: 300
    flags: Qt.Window | Qt.WindowDoesNotAcceptFocus
    Text { text: "Behind"; x: 20; y: 20 }
    Popup {
        objectName: "popup"; x: 200; y: 150; width: 100; height: 80; modal: true; closePolicy: Popup.NoAutoClose
        Text { text: "InPopup" }
    }
}
)QML";

void popupTests()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(kPopupScene, QUrl());
    std::unique_ptr<QObject> root(component.create());
    auto* window = qobject_cast<QQuickWindow*>(root.get());
    check(window != nullptr, "popupSceneLoads");
    if (window == nullptr) {
        std::fprintf(stderr, "%s\n", qPrintable(component.errorString()));
        return;
    }
    window->show();
    QCoreApplication::processEvents();
    const QList<QQuickWindow*> windows{window};
    QString error;
    UiProbe::Item behind;
    check(UiProbe::find(UiProbe::collect(windows), QJsonObject{{"text", "Behind"}}, behind, error) &&
          UiProbe::coveringItem(windows, behind) == nullptr, "noPopupNotCovered");

    QObject* popup = root->findChild<QObject*>("popup");
    check(popup != nullptr, "popupFound");
    if (popup == nullptr) {
        return;
    }
    popup->setProperty("visible", true);
    QCoreApplication::processEvents();
    check(UiProbe::find(UiProbe::collect(windows), QJsonObject{{"text", "Behind"}}, behind, error), "behindStillListed");
    QQuickItem* cover = UiProbe::coveringItem(windows, behind);
    check(cover != nullptr && cover->inherits("QQuickOverlay"), "modalPopupCoversThePage");
    UiProbe::Item inPopup;
    check(UiProbe::find(UiProbe::collect(windows), QJsonObject{{"text", "InPopup"}}, inPopup, error) &&
          UiProbe::coveringItem(windows, inPopup) == nullptr, "modalPopupContentClickable");

    popup->setProperty("modal", false);
    QCoreApplication::processEvents();
    check(UiProbe::coveringItem(windows, behind) == nullptr, "nonModalPopupDoesNotCover");
}

void pageTests()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(kStackScene, QUrl());
    std::unique_ptr<QObject> root(component.create());
    auto* window = qobject_cast<QQuickWindow*>(root.get());
    check(window != nullptr, "stackSceneLoads");
    if (window == nullptr) {
        std::fprintf(stderr, "%s\n", qPrintable(component.errorString()));
        return;
    }
    window->show();
    QCoreApplication::processEvents();
    QString title;
    bool busy = true;
    const QString page = UiProbe::currentPage(window, &title, &busy);
    check(!busy, "stackViewNotBusyWithoutTransition");
    check(page == "Rectangle", "currentPageIsTheTypeOnTopOfTheStackView");
    check(title == "Titre de la page", "currentPageTitleIsItsObjectName");
}
}

int main(int argc, char** argv)
{
    allowlistTests();
    protocolTests();

    // Offscreen, software rendering: no window on the user's screen, no focus taken
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    QGuiApplication app(argc, argv);
    uiTests();
    pageTests();
    popupTests();
    removeTreeTests();

    if (failures == 0) {
        std::printf("tst_testguard: all passed\n");
    }
    return failures == 0 ? 0 : 1;
}
