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

#ifndef TRANSCODEROPTIONSAAC_H
#define TRANSCODEROPTIONSAAC_H

#include "config.h"

#include <QWidget>

#include "transcoderoptionsinterface.h"

class Ui_TranscoderOptionsAAC;

class TranscoderOptionsAAC : public TranscoderOptionsInterface {
 public:
  TranscoderOptionsAAC(QWidget *parent = nullptr);
  ~TranscoderOptionsAAC();

  void Load();
  void Save();

private:
  static const char *kSettingsGroup;

  Ui_TranscoderOptionsAAC *ui_;
};

#endif  // TRANSCODEROPTIONSAAC_H
