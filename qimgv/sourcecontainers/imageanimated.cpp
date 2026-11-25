#include "imageanimated.h"
#include <QImageReader>
#include <QBuffer>
#include <time.h>

// TODO: this class is kinda useless now. redesign?

ImageAnimated::ImageAnimated(QString _path)
    : Image(_path)
{
    mSize.setWidth(0);
    mSize.setHeight(0);
    load();
}

ImageAnimated::ImageAnimated(std::unique_ptr<DocumentInfo> _info)
    : Image(std::move(_info))
{
    mSize.setWidth(0);
    mSize.setHeight(0);
    load();
}

ImageAnimated::~ImageAnimated() {
}

void ImageAnimated::load() {
    if(isLoaded())
        return;
    loadMovie();
    mLoaded = true;
}

void ImageAnimated::loadMovie() {
    movie.reset(new QMovie());
    if(mDocInfo->isArchive()) {
        mData = mDocInfo->archiveData();
        mBuffer.reset(new QBuffer(&mData));
        mBuffer->open(QIODevice::ReadOnly);
        movie->setDevice(mBuffer.get());
        movie->setFormat(mDocInfo->format().toLatin1());
    } else {
        movie->setFileName(mPath);
        movie->setFormat(mDocInfo->format().toStdString().c_str());
    }
    movie->jumpToFrame(0);
    mSize = movie->frameRect().size();
    mFrameCount = movie->frameCount();
}

int ImageAnimated::frameCount() {
    return mFrameCount;
}

// TODO: overwrite (self included)
bool ImageAnimated::save(QString destPath) {
    if(mDocInfo->isArchive()) {
        qDebug() << "ImageAnimated::save() - cannot save file inside archive.";
        return false;
    }
    QFile file(mPath);
    if(file.exists()) {
        if(!file.copy(destPath)) {
            qDebug() << "Unable to save file.";
            return false;
        } else {
            qDebug() << destPath << this->filePath();
            if(destPath == this->filePath()) {
                mDocInfo->refresh();
            }
            return true;
        }
    } else {
        qDebug() << "Unable to save file. Perhaps the source file was deleted?";
        return false;
    }
}

bool ImageAnimated::save() {
    //TODO
    return false;
}

// in case of gif returns current frame
std::unique_ptr<QPixmap> ImageAnimated::getPixmap() {
    if(mDocInfo->isArchive()) {
        QByteArray data = mDocInfo->archiveData();
        QBuffer buffer(&data);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer, mDocInfo->format().toLatin1());
        return std::unique_ptr<QPixmap>(new QPixmap(QPixmap::fromImage(reader.read())));
    }
    return std::unique_ptr<QPixmap>(new QPixmap(mPath, mDocInfo->format().toStdString().c_str()));
}

std::shared_ptr<const QImage> ImageAnimated::getImage() {
    if(mDocInfo->isArchive()) {
        QByteArray data = mDocInfo->archiveData();
        QBuffer buffer(&data);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer, mDocInfo->format().toLatin1());
        return std::shared_ptr<const QImage>(new QImage(reader.read()));
    }
    std::shared_ptr<const QImage> img(new QImage(mPath, mDocInfo->format().toStdString().c_str()));
    return img;
}

std::shared_ptr<QMovie> ImageAnimated::getMovie() {
    if(movie == nullptr)
        loadMovie();
    return movie;
}

int ImageAnimated::height() {
    return mSize.height();
}

int ImageAnimated::width() {
    return mSize.width();
}

QSize ImageAnimated::size() {
    return mSize;
}
