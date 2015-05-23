#include "overlayhandler.h"

#include "bakaengine.h"
#include "mpvhandler.h"
#include "ui/mainwindow.h"
#include "ui_mainwindow.h"
#include "util.h"
#include "overlay.h"

#include <QFileInfo>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QBrush>
#include <QTimer>
#include <QFontMetrics>

#define OVERLAY_INFO 62
#define OVERLAY_STATUS 63

OverlayHandler::OverlayHandler(QObject *parent):
    QObject(parent),
    baka(static_cast<BakaEngine*>(parent)),
    min_overlay(1),
    max_overlay(60),
    overlay_id(min_overlay)
{
}

OverlayHandler::~OverlayHandler()
{
    for(auto o : overlays)
        delete o;
}

void OverlayHandler::showStatusText(const QString &text, int duration)
{
    if(text != QString() && duration != 0)
        showText({text},
                 QFont(Util::MonospaceFont(),
                       14,
                       QFont::Bold), QColor(0xFFFFFF),
                 QPoint(20, 20), duration, OVERLAY_STATUS);
    else
        remove(OVERLAY_STATUS);
}

void OverlayHandler::showInfoText(bool show)
{
    if(show) // show media info
    {
        QStringList text = baka->mpv->getMediaInfo().split('\n');
        showText(text,
                 QFont(Util::MonospaceFont(),
                       int(std::min(double(baka->window->ui->mpvFrame->width() / 75),
                                    double(baka->window->ui->mpvFrame->height() / (1.55*text.length()+1)))),
                       QFont::Bold), QColor(0xFFFF00),
                 QPoint(20, 20), 0, OVERLAY_INFO);
    }
    else // hide media info
        remove(OVERLAY_INFO);
}

void OverlayHandler::showText(const QStringList &text, QFont font, QColor color, QPoint pos, int duration, int id)
{
    // increase next overlay_id
    if(id == -1) // auto id
    {
        id = overlay_id;
        if(overlay_id+1 > max_overlay)
            overlay_id = min_overlay;
        else
            ++overlay_id;
    }

    // draw text to image
    QPainterPath path(QPoint(0, 0));
    QFontMetrics fm(font);
    int h = fm.height();
    int w = 0;
    QPoint p = QPoint(0, h);
    for(auto element : text)
    {
        path.addText(p, font, element);
        if(path.currentPosition().x()+h > w)
            w = path.currentPosition().x()+h;
        p += QPoint(0, h);
    }

    QImage *canvas = new QImage(w, p.y(), QImage::Format_ARGB32); // make the canvas the right size
    canvas->fill(QColor(0,0,0,0)); // fill it with nothing

    QPainter painter(canvas); // prepare to paint
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setCompositionMode(QPainter::CompositionMode_Overlay);
    painter.setFont(font);
    painter.setPen(QColor(0, 0, 0));
    painter.setBrush(color);
    painter.drawPath(path);

    // add over mpv as label
    QLabel *label = new QLabel(baka->window->ui->mpvFrame);
    label->setStyleSheet("background-color:rgb(0,0,0,0);background-image:url();");
    label->setGeometry(pos.x(),
                       pos.y(),
                       canvas->width(),
                       canvas->height());
    label->setPixmap(QPixmap::fromImage(*canvas));
    label->show();

    QTimer *timer;
    if(duration == 0)
        timer = nullptr;
    else
    {
        timer = new QTimer(this);
        timer->start(duration);
        connect(timer, &QTimer::timeout, // on timeout
                [=] { remove(id); });
    }

    // add as mpv overlay
    baka->mpv->AddOverlay(
        id == -1 ? overlay_id : id,
        pos.x(), pos.y(),
        "&"+QString::number(quintptr(canvas->bits())),
        0, canvas->width(), canvas->height());

    if(overlays.find(id) != overlays.end())
        delete overlays[id];
    overlays[id] = new Overlay(label, canvas, timer, this);
}

void OverlayHandler::remove(int id)
{
    baka->mpv->RemoveOverlay(id);
    if(overlays.find(id) != overlays.end())
    {
        delete overlays[id];
        overlays.remove(id);
    }
}
