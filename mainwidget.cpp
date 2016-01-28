#include "mainwidget.h"
#include "ui_mainwidget.h"
#include <stdio.h>
#include <QDir>
#include <sstream>
#include <QString>
#include <QFile>
#include <QIODevice>
#include <QTextStream>
#include <QPushButton>
#include <QPainter>
#ifdef WIN32
#   define snprintf _snprintf
#endif


MainWidget::MainWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MainWidget)
{
    ui->setupUi(this);

    load_image_fnames();
    load_catalogs();

    QObject::connect(ui->button_undo, SIGNAL(clicked(void)), this, SLOT(undo()));

    // 根据类别，加载按钮.
    for (size_t i = 0; i < catalogs_.size(); i++) {
        QString title(catalogs_[i]);
        QPushButton *but = new QPushButton(title);
        ui->verticalLayout_buttons->addWidget(but);
        buttons_.push_back(but);

        QObject::connect(but, SIGNAL(clicked(void)), this, SLOT(but_clicked(void)));
    }

    QPixmap *img = next_image();
    if (img) {
        show_image(img);
        delete img;
    }

    disable_buttons();

    show_info();
}

MainWidget::~MainWidget()
{
    delete ui;
}

void MainWidget::paintEvent(QPaintEvent *pd)
{
    if (!curr_image_.isNull()) {
        QPainter p(this);
        QImage img = curr_image_.toImage();
        p.drawImage(ui->widget_show->rect(), img);
    }
}

void MainWidget::disable_buttons()
{
    for (size_t i = 0; i < buttons_.size(); i++) {
        QPushButton *but = buttons_[i];
        but->setEnabled(!img_fnames_.empty());
    }

    ui->button_undo->setEnabled(!undo_list_.empty());
}

void MainWidget::load_catalogs()
{
    QFile file(CATALOG_FNAME);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream ts(&file);
        QDir curr_dir("./");

        while (!ts.atEnd()) {
            QString line = ts.readLine().simplified();
            if (line.count() > 2) {
                catalogs_.push_back(line);

                // 创建子目录.
                QString fname = IMG_PATH;
                fname += '/';
                fname += line;

                curr_dir.mkdir(fname);
            }
        }

        file.close();
    }


    fprintf(stderr, "load_catalogs: load %lu catalogs\n", catalogs_.size());
}

void MainWidget::load_image_fnames()
{
    QDir dir(IMG_PATH);
    QStringList filters;
    filters << "*.jpg" << "*.JPG" << "*.png" << "*.PNG";
    dir.setNameFilters(filters);

    QStringList list = dir.entryList();
    QStringList::const_iterator it;
    for (it = list.cbegin(); it != list.cend(); ++it) {
        QString fname = IMG_PATH;
        fname += '/';
        fname += *it;

        img_fnames_.push_back(fname);
    }

    fprintf(stderr, "%lu images fname loaded!\n", img_fnames_.size());
}

void MainWidget::show_image(const QPixmap *img)
{
    curr_image_ = *img;
    ui->widget_show->update();
}

QPixmap *MainWidget::next_image()
{
    while (!img_fnames_.empty()) {
        QString fname = img_fnames_.front();
        QPixmap *img = new QPixmap(fname);
        if (img->isNull()) {
            fprintf(stderr, "WRN: '%s' can't load!\n", fname.toLocal8Bit().constData());
            img_fnames_.pop_front();
            continue;
        }

        return img;
    }

    fprintf(stderr, "INFO: NO more images!\n");
    return 0;
}

void MainWidget::show_curr()
{
    QPixmap *img = next_image();
    if (img) {
        show_image(img);
        delete img;
    }
}

void MainWidget::but_clicked()
{
    if (img_fnames_.empty()) {
        return; // TODO: 应该禁用按钮 ...
    }

    QPushButton *but = (QPushButton*)sender();
    QString title = but->text();
    QString catalog = title;
    fprintf(stderr, "catalog: '%s' selected!\n", catalog.toLocal8Bit().constData());

    /** 将当前文件移动到 catalog 对应的子目录中.
     *  从 img_fnames_.front() 删除，保存到 undo_list_ 中，用于支持undo
     */
    QString src_fname = img_fnames_.front();
    QString dst_fname = cataloged_fname(catalog, src_fname);
    QFile::rename(src_fname, dst_fname);

    undo_list_.push(dst_fname);
    img_fnames_.pop_front();

    disable_buttons();

    show_curr();
    show_info();
}

void MainWidget::undo()
{
    if (undo_list_.empty()) {
        return;
    }

    QString src_fname = undo_list_.top();
    QString dst_fname = origin_fname(src_fname);
    QFile::rename(src_fname, dst_fname);

    undo_list_.pop();
    img_fnames_.push_front(dst_fname);

    disable_buttons();

    show_curr();
    show_info();
}

void MainWidget::show_info()
{
    char buf[128];
    snprintf(buf, sizeof(buf), "类别: %u, 剩余: %u", catalogs_.size(), img_fnames_.size());
    setWindowTitle(buf);
}
