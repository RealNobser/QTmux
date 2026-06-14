#include <QtGui/QGuiApplication>
#include <QtSvg/QSvgRenderer>
#include <QtGui/QImage>
#include <QtGui/QPainter>
int main(int argc, char** argv){
    QGuiApplication app(argc, argv);
    if(argc<4) return 2;
    QString svg=argv[1], out=argv[2]; int sz=QString(argv[3]).toInt();
    QSvgRenderer r((QString(svg)));
    if(!r.isValid()){ fprintf(stderr,"invalid svg\n"); return 1; }
    QImage img(sz,sz,QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing,true);
    p.setRenderHint(QPainter::SmoothPixmapTransform,true);
    r.render(&p, QRectF(0,0,sz,sz));
    p.end();
    if(!img.save(out)){ fprintf(stderr,"save failed\n"); return 1; }
    return 0;
}
