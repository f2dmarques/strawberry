/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 *
 * Strawberry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Strawberry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef RATINGWIDGET_H
#define RATINGWIDGET_H

#include "config.h"

#include <QObject>
#include <QWidget>
#include <QString>
#include <QPixmap>
#include <QPainter>
#include <QRect>
#include <QSize>
#include <QPoint>
#include <QtEvents>

class QEvent;
class QMouseEvent;
class QPaintEvent;

class RatingPainter {
public:
  RatingPainter();

  static const int kStarCount = 5;
  static const int kStarSize = 15;
  static QRect Contents(const QRect& rect);
  static double RatingForPos(const QPoint& pos, const QRect& rect);

  void Paint(QPainter *painter, const QRect& rect, float rating) const;

private:
  QPixmap stars_[kStarCount*2+1];
};

class RatingWidget : public QWidget {
  Q_OBJECT
  Q_PROPERTY(float rating READ rating WRITE set_rating);

 public:
  RatingWidget(QWidget *parent = nullptr);

  QSize sizeHint() const;

  float rating() const { return rating_; }
  void set_rating(float rating);

signals:
  void RatingChanged(float rating);

protected:
  void paintEvent(QPaintEvent*);
  void mousePressEvent(QMouseEvent *e);
  void mouseMoveEvent(QMouseEvent *e);
  void leaveEvent(QEvent*);

private:
  RatingPainter painter_;
  float rating_;
  float hover_rating_;
};

#endif  // RATINGWIDGET_H

