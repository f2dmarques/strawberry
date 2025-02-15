/* This file is part of Strawberry.
   Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>

   Strawberry is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Strawberry is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"

#include <string>

#include "tagreaderbase.h"

const std::string TagReaderBase::kEmbeddedCover = "(embedded)";

TagReaderBase::TagReaderBase() = default;
TagReaderBase::~TagReaderBase() = default;

void TagReaderBase::Decode(const QString &tag, std::string *output) {

  output->assign(DataCommaSizeFromQString(tag));

}

float TagReaderBase::ConvertPOPMRating(const int POPM_rating) {

  if (POPM_rating < 0x01) return 0.0F;
  else if (POPM_rating < 0x40) return 0.20F;
  else if (POPM_rating < 0x80) return 0.40F;
  else if (POPM_rating < 0xC0) return 0.60F;
  else if (POPM_rating < 0xFC) return 0.80F;

  return 1.0F;

}

int TagReaderBase::ConvertToPOPMRating(const float rating) {

  if (rating < 0.20) return 0x00;
  else if (rating < 0.40) return 0x01;
  else if (rating < 0.60) return 0x40;
  else if (rating < 0.80) return 0x80;
  else if (rating < 1.0)  return 0xC0;

  return 0xFF;

}
