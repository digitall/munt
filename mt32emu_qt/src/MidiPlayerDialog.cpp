/* Copyright (C) 2011, 2012 Jerome Fisher, Sergey V. Mikayev
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QFileDialog>

#include "MidiPlayerDialog.h"

static const quint32 DEFAULT_BPM = 120;

MidiPlayerDialog::MidiPlayerDialog(Master *master, QWidget *parent) : QDialog(parent), ui(new Ui::MidiPlayerDialog), smfDriver(master), advancePlayList(false), rowPlaying(-1) {
	ui->setupUi(this);
	ui->playButton->setEnabled(false);
	ui->positionSlider->setTracking(false);
	connect(&smfDriver, SIGNAL(playbackFinished()), SLOT(handlePlaybackFinished()));
	connect(&smfDriver, SIGNAL(playbackTimeChanged(quint64, quint32)), SLOT(handlePlaybackTimeChanged(quint64, quint32)));
	connect(&smfDriver, SIGNAL(tempoUpdated(quint32)), SLOT(handleTempoSet(quint32)));
}

MidiPlayerDialog::~MidiPlayerDialog() {
	delete ui;
}

void MidiPlayerDialog::on_addButton_clicked() {
	static QString currentDir = NULL;
	QStringList fileNames = QFileDialog::getOpenFileNames(NULL, NULL, currentDir, "*.mid *.smf *.syx;;*.mid;;*.smf;;*.syx;;*.*");
	if (!fileNames.isEmpty()) {
		currentDir = QDir(fileNames.first()).absolutePath();
		ui->playList->addItems(fileNames);
	}
	ui->playButton->setEnabled(ui->playList->count() > 0);
}

void MidiPlayerDialog::on_addListButton_clicked() {
	static QString currentDir = NULL;
	QString fileName = QFileDialog::getOpenFileName(NULL, NULL, currentDir, "Play list files (*.*)");
	if (!fileName.isEmpty()) {
		currentDir = QDir(fileName).absolutePath();
		QFile listFile(fileName);
		if (!listFile.open(QIODevice::ReadOnly)) return;
		QTextStream listStream(&listFile);
		while (!listStream.atEnd()) {
			QString s = listStream.readLine();
			if (s.isEmpty()) continue;
			ui->playList->addItem(s);
		}
	}
	ui->playButton->setEnabled(ui->playList->count() > 0);
}

void MidiPlayerDialog::on_removeButton_clicked() {
	ui->playList->takeItem(ui->playList->currentRow());
	ui->playButton->setEnabled(ui->playList->count() > 0);
}

void MidiPlayerDialog::on_clearButton_clicked() {
	ui->playList->clear();
	ui->playButton->setEnabled(false);
}

void MidiPlayerDialog::on_saveListButton_clicked() {
	static QString currentDir = NULL;
	QString fileName = QFileDialog::getSaveFileName(NULL, NULL, currentDir, "Play list files (*.*)");
	if (!fileName.isEmpty()) {
		currentDir = QDir(fileName).absolutePath();
		QFile listFile(fileName);
		if (!listFile.open(QIODevice::WriteOnly)) return;
		QTextStream listStream(&listFile);
		for (int i = 0; i < ui->playList->count(); i++) {
			listStream << ui->playList->item(i)->text() << endl;
		}
	}
}

void MidiPlayerDialog::on_moveUpButton_clicked() {
	int currentRow = ui->playList->currentRow();
	if (currentRow < 1) return;
	QString currentFile = ui->playList->currentItem()->text();
	QString prevFile = ui->playList->item(currentRow - 1)->text();
	ui->playList->currentItem()->setText(prevFile);
	ui->playList->item(currentRow - 1)->setText(currentFile);
	ui->playList->setCurrentRow(currentRow - 1);
}

void MidiPlayerDialog::on_moveDownButton_clicked() {
	int currentRow = ui->playList->currentRow();
	if ((currentRow == -1) || ((ui->playList->count() - 2) < currentRow)) return;
	QString currentFile = ui->playList->currentItem()->text();
	QString nextFile = ui->playList->item(currentRow + 1)->text();
	ui->playList->currentItem()->setText(nextFile);
	ui->playList->item(currentRow + 1)->setText(currentFile);
	ui->playList->setCurrentRow(currentRow + 1);
}

void MidiPlayerDialog::on_playButton_clicked() {
	if (rowPlaying != -1) {
		smfDriver.stop();
	}
	rowPlaying = ui->playList->currentRow();
	advancePlayList = (rowPlaying == -1);
	handlePlaybackFinished();
}

void MidiPlayerDialog::on_stopButton_clicked() {
	if (rowPlaying != -1) {
		smfDriver.stop();
	}
	rowPlaying = -1;
	ui->tempoSpinBox->setValue(DEFAULT_BPM);
}

void MidiPlayerDialog::on_fastForwardButton_pressed() {
	smfDriver.setFastForwardingFactor(4);
}

void MidiPlayerDialog::on_fastForwardButton_released() {
	smfDriver.setFastForwardingFactor(0);
}

void MidiPlayerDialog::on_fastFastForwardButton_pressed() {
	smfDriver.setFastForwardingFactor(16);
}

void MidiPlayerDialog::on_fastFastForwardButton_released() {
	smfDriver.setFastForwardingFactor(0);
}

void MidiPlayerDialog::on_tempoSpinBox_valueChanged(int newValue) {
	smfDriver.setBPM(ui->tempoSpinBox->value());
}

void MidiPlayerDialog::on_positionSlider_valueChanged() {
	smfDriver.jump(ui->positionSlider->value());
}

void MidiPlayerDialog::handlePlaybackFinished() {
	if (advancePlayList) {
		if (ui->playList->count() <= ++rowPlaying) {
			rowPlaying = -1;
			ui->playList->clearSelection();
			ui->tempoSpinBox->setValue(DEFAULT_BPM);
			return;
		}
		ui->playList->setCurrentRow(rowPlaying);
	}
	advancePlayList = true;
	ui->tempoSpinBox->setValue(DEFAULT_BPM);
	smfDriver.start(ui->playList->currentItem()->text());
}

void MidiPlayerDialog::handlePlaybackTimeChanged(quint64 currentNanos, quint32 totalSeconds) {
	quint32 currentSeconds = currentNanos / MasterClock::NANOS_PER_SECOND;
	QChar z = QChar('0');
	QString pos = QString("%1:%2 / %3:%4").arg(currentSeconds / 60, 2, 10, z).arg(currentSeconds % 60, 2, 10, z).arg(totalSeconds / 60, 2, 10, z).arg(totalSeconds % 60, 2, 10, z);
	ui->positionLabel->setText(pos);
	ui->positionSlider->setSliderPosition((totalSeconds != 0) ? currentNanos / MasterClock::NANOS_PER_MILLISECOND / totalSeconds : 0);
}

void MidiPlayerDialog::handleTempoSet(quint32 tempo) {
	ui->tempoSpinBox->setValue(tempo);
}