/******************************************************************************
    Copyright (C) 2026 OBS Studio contributors

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "TestWindow.hpp"

#include <OBSApp.hpp>

#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

#include "moc_TestWindow.cpp"

namespace OBS {

TestWindow::TestWindow(QWidget *parent) : QDialog(parent)
{
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowTitle(QTStr("TestWindow"));
	resize(640, 400);

	auto *layout = new QVBoxLayout(this);
	auto *description = new QLabel(QTStr("TestWindow.Description"), this);
	description->setWordWrap(true);
	layout->addWidget(description);

	operationsLayout_ = new QVBoxLayout;
	operationsLayout_->setObjectName("testOperationsLayout");
	layout->addLayout(operationsLayout_);
	layout->addStretch();

	auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttonBox);
}

} // namespace OBS
