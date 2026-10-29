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
#include <utility/Minidump.hpp>

#include <util/util.hpp>

#include <QDialogButtonBox>
#include <QDateTime>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <filesystem>

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
	captureMinidumpButton_ = new QPushButton(QTStr("TestWindow.CaptureMinidump"), this);
	connect(captureMinidumpButton_, &QPushButton::clicked, this, &TestWindow::captureMinidump);
	operationsLayout_->addWidget(captureMinidumpButton_);

	minidumpResultLabel_ = new QLabel(this);
	minidumpResultLabel_->setWordWrap(true);
	operationsLayout_->addWidget(minidumpResultLabel_);
	layout->addLayout(operationsLayout_);
	layout->addStretch();

	auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttonBox);
}

void TestWindow::captureMinidump()
{
	BPtr<char> crashDirectoryPath(GetAppConfigPathPtr("obs-studio/crashes"));
	if (!crashDirectoryPath) {
		minidumpResultLabel_->setText(QTStr("TestWindow.Minidump.PathError"));
		return;
	}

	const std::filesystem::path crashDirectory = std::filesystem::u8path(crashDirectoryPath.Get());
	std::error_code error;
	std::filesystem::create_directories(crashDirectory, error);
	if (error) {
		minidumpResultLabel_->setText(QTStr("TestWindow.Minidump.PathError"));
		return;
	}

	const QString fileName = QString("obs-minidump-%1.dmp")
					 .arg(QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss-zzz"));
	const std::filesystem::path dumpPath = crashDirectory / std::filesystem::u8path(fileName.toStdString());
	const OBS::MinidumpResult result = OBS::CaptureCurrentProcessMinidump(dumpPath);

	switch (result.status) {
	case OBS::MinidumpStatus::Success:
		minidumpResultLabel_->setText(QTStr("TestWindow.Minidump.Success")
					      .arg(QString::fromStdString(dumpPath.u8string())));
		break;
	case OBS::MinidumpStatus::Unsupported:
		minidumpResultLabel_->setText(QTStr("TestWindow.Minidump.Unsupported"));
		break;
	case OBS::MinidumpStatus::LoaderUnavailable:
	case OBS::MinidumpStatus::FileOpenFailed:
	case OBS::MinidumpStatus::WriteFailed:
		minidumpResultLabel_->setText(QTStr("TestWindow.Minidump.Failed")
					      .arg(QString::number(result.errorCode)));
		break;
	}
}

} // namespace OBS
