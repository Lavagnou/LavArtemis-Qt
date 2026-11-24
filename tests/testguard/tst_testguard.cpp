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
}

const char* kScene = R"QML(
import QtQuick
import QtQuick.Window
Window {
    id: root
    width: 400; height: 300
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

    UiProbe::click(window, bonjour.rect.center(), Qt::LeftButton);
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

    if (failures == 0) {
        std::printf("tst_testguard: all passed\n");
    }
    return failures == 0 ? 0 : 1;
}
