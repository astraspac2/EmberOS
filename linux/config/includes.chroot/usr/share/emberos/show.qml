import QtQuick 2.0
import calamares.slideshow 1.0

Presentation {
    id: presentation

    function nextSlide() { presentation.goToNextSlide() }
    Timer { interval: 6000; running: true; repeat: true; onTriggered: nextSlide() }

    Slide {
        Rectangle { anchors.fill: parent; color: "#1A1210" }
        Image { id: logo; source: "logo.png"; width: 160; height: 160
                anchors.horizontalCenter: parent.horizontalCenter; y: parent.height * 0.14 }
        Text { anchors.horizontalCenter: parent.horizontalCenter; y: parent.height * 0.14 + 190
               text: "Welcome to EmberOS"; color: "#FF9628"; font.pixelSize: 30 }
        Text { anchors.horizontalCenter: parent.horizontalCenter; y: parent.height * 0.14 + 245
               text: "A warm, calm desktop built on Debian."; color: "#F0E4DA"; font.pixelSize: 17 }
    }
    Slide {
        Rectangle { anchors.fill: parent; color: "#1A1210" }
        Text { anchors.centerIn: parent; horizontalAlignment: Text.AlignHCenter
               text: "Get apps in one click\n\nOpen Software from the menu to install\nthousands of apps, including from Flathub."
               color: "#F0E4DA"; font.pixelSize: 20 }
    }
    Slide {
        Rectangle { anchors.fill: parent; color: "#1A1210" }
        Text { anchors.centerIn: parent; horizontalAlignment: Text.AlignHCenter
               text: "Make it yours\n\nPick a wallpaper with the Wallpapers app,\nor tweak everything in Settings."
               color: "#F0E4DA"; font.pixelSize: 20 }
    }
    Slide {
        Rectangle { anchors.fill: parent; color: "#1A1210" }
        Text { anchors.centerIn: parent; horizontalAlignment: Text.AlignHCenter
               text: "Safe by default\n\nA firewall is on and security updates\ninstall themselves."
               color: "#F0E4DA"; font.pixelSize: 20 }
    }
}
