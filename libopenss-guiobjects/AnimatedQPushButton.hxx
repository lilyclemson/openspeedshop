////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2005 Silicon Graphics, Inc. All Rights Reserved.
//
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the Free
// Software Foundation; either version 2.1 of the License, or (at your option)
// any later version.
//
// This library is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
// details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this library; if not, write to the Free Software Foundation, Inc.,
// 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
////////////////////////////////////////////////////////////////////////////////


#ifndef ANIMATEDQPUSHBUTTON_H
#define ANIMATEDQPUSHBUTTON_H

#include <qpushbutton.h>
#include <qvaluelist.h>
#include <qiconset.h>

typedef QValueList<QPixmap *>ImageList;

class AnimatedQPushButton : public QPushButton
{
  public:
    AnimatedQPushButton(QIconSet, QString, QWidget *, bool f=TRUE);
    AnimatedQPushButton(QWidget *, const char *, bool f=TRUE);

    ~AnimatedQPushButton();

    void push_back(QPixmap *);
    void remove(QPixmap *);

    ImageList imageList;

    void enterEvent ( QEvent * ); 
    void leaveEvent ( QEvent * );

    bool enabledFLAG;
  public slots:

  private:
};
#endif // ANIMATEDQPUSHBUTTON_H
