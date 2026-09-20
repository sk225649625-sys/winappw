#include "MainWindow.h"
#include "../media/AppPaths.h"
#include "../media/MediaStore.h"
#include "../media/ProjectStore.h"
#include "../core/RenderEngine.h"
#include "../core/Catalogs.h"
#include "../core/Json.h"

#include <QWidget>
#include <QListWidget>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QProgressDialog>
#include <QSplitter>
#include <QStatusBar>
#include <QMenuBar>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMetaObject>
#include <thread>

namespace rf {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    AppPaths::ensureFolders();
    detectTools();
    m_media = std::make_unique<MediaStore>(m_ffmpeg, m_ffprobe);
    m_project = ProjectStore::blankProject();
    m_currentProjectName = "project.json";
    setupUi();
    refreshMediaList();
    refreshTimelineList();
}

MainWindow::~MainWindow() {
    if (m_renderEngine) m_renderEngine->cancel();
}

void MainWindow::detectTools() {
    m_ffmpeg = AppPaths::findTool("ffmpeg");
    m_ffprobe = AppPaths::findTool("ffprobe");
    if (m_ffmpeg.isEmpty() || m_ffprobe.isEmpty()) {
        QMessageBox::critical(this, "ReelForge",
            "ffmpeg/ffprobe nahi mila.\n\n"
            "Ye folder check karo: " + AppPaths::ffmpegDir() + "\\bin\n"
            "Ya ffmpeg.exe + ffprobe.exe ReelForge.exe ke bagal me rakh do.");
    }
    m_font = AppPaths::findFont({ "segoeuib.ttf", "arialbd.ttf", "Poppins-Bold.ttf", "NotoSans-Bold.ttf" });
    m_emojiFont = AppPaths::findFont({ "seguiemj.ttf", "NotoColorEmoji.ttf" });
}

void MainWindow::setupUi() {
    setWindowTitle(QString("ReelForge  --  editing aur export dono is computer par  (Cores: %1)").arg(Catalogs::cpuCount()));
    resize(1500, 900);

    auto* central = new QWidget(this);
    auto* mainLayout = new QHBoxLayout(central);

    // ---- left: media bin ----
    auto* leftBox = new QGroupBox("Media (local disk)", central);
    auto* leftLayout = new QVBoxLayout(leftBox);
    m_mediaList = new QListWidget(leftBox);
    m_mediaList->setIconSize({ 96, 72 });
    auto* importBtn = new QPushButton("Import photos/audio...", leftBox);
    auto* addBtn = new QPushButton("+ Timeline me daalo", leftBox);
    leftLayout->addWidget(importBtn);
    leftLayout->addWidget(m_mediaList, 1);
    leftLayout->addWidget(addBtn);
    connect(importBtn, &QPushButton::clicked, this, &MainWindow::onImportMedia);
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::onAddToTimeline);
    connect(m_mediaList, &QListWidget::itemClicked, this, &MainWindow::onMediaSelected);

    // ---- center: timeline + preview ----
    auto* centerBox = new QGroupBox("Timeline", central);
    auto* centerLayout = new QVBoxLayout(centerBox);
    m_previewLabel = new QLabel(centerBox);
    m_previewLabel->setMinimumHeight(320);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("background:#111;color:#888;");
    m_previewLabel->setText("Preview yahan dikhega");
    m_timelineList = new QListWidget(centerBox);
    auto* tlBtns = new QHBoxLayout();
    auto* upBtn = new QPushButton("Upar", centerBox);
    auto* downBtn = new QPushButton("Neeche", centerBox);
    auto* delBtn = new QPushButton("Hatao", centerBox);
    tlBtns->addWidget(upBtn); tlBtns->addWidget(downBtn); tlBtns->addWidget(delBtn);
    centerLayout->addWidget(m_previewLabel, 1);
    centerLayout->addWidget(m_timelineList, 1);
    centerLayout->addLayout(tlBtns);
    connect(m_timelineList, &QListWidget::itemClicked, this, &MainWindow::onClipSelected);
    connect(upBtn, &QPushButton::clicked, this, &MainWindow::onMoveUp);
    connect(downBtn, &QPushButton::clicked, this, &MainWindow::onMoveDown);
    connect(delBtn, &QPushButton::clicked, this, &MainWindow::onRemoveClip);

    // ---- right: properties + export ----
    auto* rightBox = new QGroupBox("Clip properties", central);
    auto* rightLayout = new QVBoxLayout(rightBox);
    auto* form = new QFormLayout();
    m_motionBox = new QComboBox(rightBox);
    m_motionBox->addItems(Catalogs::motions());
    m_transBox = new QComboBox(rightBox);
    for (auto k : Catalogs::transMap().keys()) m_transBox->addItem(k);
    m_lookBox = new QComboBox(rightBox);
    for (auto k : Catalogs::looks().keys()) m_lookBox->addItem(k);
    m_durSpin = new QDoubleSpinBox(rightBox);
    m_durSpin->setRange(0.2, 60.0);
    m_durSpin->setSingleStep(0.1);
    m_durSpin->setValue(3.0);
    m_captionEdit = new QLineEdit(rightBox);
    form->addRow("Motion", m_motionBox);
    form->addRow("Transition", m_transBox);
    form->addRow("Look", m_lookBox);
    form->addRow("Duration (s)", m_durSpin);
    form->addRow("Caption", m_captionEdit);
    rightLayout->addLayout(form);
    connect(m_motionBox, &QComboBox::currentTextChanged, this, &MainWindow::onClipPropsChanged);
    connect(m_transBox, &QComboBox::currentTextChanged, this, &MainWindow::onClipPropsChanged);
    connect(m_lookBox, &QComboBox::currentTextChanged, this, &MainWindow::onClipPropsChanged);
    connect(m_durSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::onClipPropsChanged);
    connect(m_captionEdit, &QLineEdit::editingFinished, this, &MainWindow::onClipPropsChanged);

    rightLayout->addStretch(1);
    auto* newBtn = new QPushButton("Naya project", rightBox);
    auto* saveBtn = new QPushButton("Save project", rightBox);
    auto* loadBtn = new QPushButton("Load project", rightBox);
    auto* previewBtn = new QPushButton("Quick Preview", rightBox);
    auto* exportBtn = new QPushButton("EXPORT (final video)", rightBox);
    exportBtn->setStyleSheet("font-weight:bold;background:#2e7d32;color:white;padding:8px;");
    rightLayout->addWidget(newBtn);
    rightLayout->addWidget(saveBtn);
    rightLayout->addWidget(loadBtn);
    rightLayout->addWidget(previewBtn);
    rightLayout->addWidget(exportBtn);
    connect(newBtn, &QPushButton::clicked, this, &MainWindow::onNewProject);
    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::onSaveProject);
    connect(loadBtn, &QPushButton::clicked, this, &MainWindow::onLoadProject);
    connect(previewBtn, &QPushButton::clicked, this, [this]() {
        QString out = QDir(AppPaths::cache()).filePath("preview-quick.mp4");
        startRender(true, out);
    });
    connect(exportBtn, &QPushButton::clicked, this, &MainWindow::onExport);

    auto* splitter = new QSplitter(central);
    splitter->addWidget(leftBox);
    splitter->addWidget(centerBox);
    splitter->addWidget(rightBox);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 5);
    splitter->setStretchFactor(2, 2);
    mainLayout->addWidget(splitter);

    setCentralWidget(central);

    m_statusLabel = new QLabel(this);
    statusBar()->addWidget(m_statusLabel);
    m_statusLabel->setText(QString("ffmpeg: %1").arg(m_ffmpeg.isEmpty() ? "NAHI MILA" : "OK"));

    auto* fileMenu = menuBar()->addMenu("File");
    fileMenu->addAction("Media folder kholo", this, [this]() { QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::media())); });
    fileMenu->addAction("Exports folder kholo", this, [this]() { QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::exports())); });
}

// ---------------------------------------------------------------- media
void MainWindow::onImportMedia() {
    QStringList files = QFileDialog::getOpenFileNames(this, "Photos/Audio chuno (local files)", QString(),
        "Media (*.jpg *.jpeg *.png *.webp *.gif *.bmp *.mp3 *.wav *.m4a *.aac *.ogg *.flac)");
    for (const auto& src : files) {
        QFileInfo fi(src);
        QString dest = QDir(AppPaths::media()).filePath(fi.fileName());
        int k = 1;
        while (QFile::exists(dest)) {
            dest = QDir(AppPaths::media()).filePath(fi.completeBaseName() + QString("_%1.").arg(k) + fi.suffix());
            k++;
        }
        QFile::copy(src, dest); // local-to-local copy, koi network nahi
    }
    refreshMediaList();
}

void MainWindow::refreshMediaList() {
    m_mediaList->clear();
    QJsonArray media = m_media->listMedia();
    for (const auto& mv : media) {
        QJsonObject m = mv.toObject();
        QString name = m.value("name").toString();
        auto* item = new QListWidgetItem(name, m_mediaList);
        QString thumb = m_media->thumbFor(name);
        if (!thumb.isEmpty()) item->setIcon(QIcon(thumb));
        item->setData(Qt::UserRole, name);
    }
}

void MainWindow::onMediaSelected(QListWidgetItem*) { /* preview on demand via double-click could be added */ }

void MainWindow::onAddToTimeline() {
    auto* item = m_mediaList->currentItem();
    if (!item) return;
    QString name = item->data(Qt::UserRole).toString();
    if (!MediaStore::isImage(name)) {
        QMessageBox::information(this, "ReelForge", "Timeline me sirf photos jaati hain (audio alag track hai).");
        return;
    }
    QJsonArray clips = J::arr(m_project, "clips");
    QJsonObject clip;
    clip["file"] = name;
    clip["dur"] = 3.0;
    clip["motion"] = "kenIn";
    clip["trans"] = "fade";
    clip["look"] = "none";
    clip["text"] = "";
    clips.append(clip);
    m_project["clips"] = clips;
    refreshTimelineList();
}

// ---------------------------------------------------------------- timeline
void MainWindow::refreshTimelineList() {
    m_timelineList->clear();
    QJsonArray clips = J::arr(m_project, "clips");
    int i = 0;
    for (const auto& cv : clips) {
        QJsonObject c = cv.toObject();
        QString label = QString("%1. %2  (%3s, %4, %5)")
            .arg(i + 1).arg(c.value("file").toString())
            .arg(J::num(c, "dur", 3), 0, 'f', 1)
            .arg(c.value("motion").toString())
            .arg(c.value("trans").toString());
        auto* item = new QListWidgetItem(label, m_timelineList);
        item->setData(Qt::UserRole, i);
        i++;
    }
    m_statusLabel->setText(QString("Clips: %1").arg(clips.size()));
}

void MainWindow::onClipSelected(QListWidgetItem* item) {
    if (!item) return;
    m_selectedClip = item->data(Qt::UserRole).toInt();
    loadClipIntoProps(m_selectedClip);
    refreshPreview();
}

void MainWindow::loadClipIntoProps(int idx) {
    QJsonArray clips = J::arr(m_project, "clips");
    if (idx < 0 || idx >= clips.size()) return;
    QJsonObject c = clips[idx].toObject();
    const QSignalBlocker b1(m_motionBox), b2(m_transBox), b3(m_lookBox), b4(m_durSpin), b5(m_captionEdit);
    m_motionBox->setCurrentText(J::str(c, "motion", "kenIn"));
    m_transBox->setCurrentText(J::str(c, "trans", "fade"));
    m_lookBox->setCurrentText(J::str(c, "look", "none"));
    m_durSpin->setValue(J::num(c, "dur", 3));
    m_captionEdit->setText(J::str(c, "text", ""));
}

void MainWindow::onClipPropsChanged() {
    if (m_selectedClip < 0) return;
    QJsonArray clips = J::arr(m_project, "clips");
    if (m_selectedClip >= clips.size()) return;
    QJsonObject c = clips[m_selectedClip].toObject();
    c["motion"] = m_motionBox->currentText();
    c["trans"] = m_transBox->currentText();
    c["look"] = m_lookBox->currentText();
    c["dur"] = m_durSpin->value();
    c["text"] = m_captionEdit->text();
    clips[m_selectedClip] = c;
    m_project["clips"] = clips;
    refreshTimelineList();
    m_timelineList->setCurrentRow(m_selectedClip);
    refreshPreview();
}

void MainWindow::onMoveUp() {
    if (m_selectedClip <= 0) return;
    QJsonArray clips = J::arr(m_project, "clips");
    QJsonValue clip = clips.takeAt(m_selectedClip);
    clips.insert(m_selectedClip - 1, clip);
    m_project["clips"] = clips;
    m_selectedClip--;
    refreshTimelineList();
    m_timelineList->setCurrentRow(m_selectedClip);
}

void MainWindow::onMoveDown() {
    QJsonArray clips = J::arr(m_project, "clips");
    if (m_selectedClip < 0 || m_selectedClip >= clips.size() - 1) return;
    QJsonValue clip = clips.takeAt(m_selectedClip);
    clips.insert(m_selectedClip + 1, clip);
    m_project["clips"] = clips;
    m_selectedClip++;
    refreshTimelineList();
    m_timelineList->setCurrentRow(m_selectedClip);
}

void MainWindow::onRemoveClip() {
    QJsonArray clips = J::arr(m_project, "clips");
    if (m_selectedClip < 0 || m_selectedClip >= clips.size()) return;
    clips.removeAt(m_selectedClip);
    m_project["clips"] = clips;
    m_selectedClip = -1;
    refreshTimelineList();
    m_previewLabel->setText("Preview yahan dikhega");
}

// ---------------------------------------------------------------- preview
void MainWindow::refreshPreview() {
    if (m_ffmpeg.isEmpty() || J::arr(m_project, "clips").isEmpty()) return;
    try {
        RenderEngine eng(m_ffmpeg, m_ffprobe, m_project, "", AppPaths::media(), true, m_font, m_emojiFont, 1);
        QString out = QDir(AppPaths::cache()).filePath("frame.jpg");
        // selected clip ke start time par frame nikalo
        auto td = eng.timeline();
        double t = 0;
        for (int i = 0; i < m_selectedClip && i < td.starts.size(); i++) t = td.starts[i];
        if (m_selectedClip >= 0 && m_selectedClip < td.starts.size()) t = td.starts[m_selectedClip];
        eng.still(t, out, 720);
        QPixmap pix(out);
        if (!pix.isNull()) m_previewLabel->setPixmap(pix.scaled(m_previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } catch (const std::exception& e) {
        m_previewLabel->setText(QString("Preview error: %1").arg(e.what()));
    }
}

// ---------------------------------------------------------------- project
QJsonObject MainWindow::currentProject() const { return m_project; }

void MainWindow::onNewProject() {
    m_project = ProjectStore::blankProject();
    m_selectedClip = -1;
    refreshTimelineList();
    m_previewLabel->setText("Preview yahan dikhega");
}

void MainWindow::onSaveProject() {
    bool ok = false;
    QString name = QInputDialog::getText(this, "Save project", "Naam:", QLineEdit::Normal, m_currentProjectName, &ok);
    if (!ok || name.isEmpty()) return;
    name = ProjectStore::projectName(name);
    QFile f(QDir(AppPaths::projects()).filePath(name));
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(m_project).toJson(QJsonDocument::Indented));
        m_currentProjectName = name;
        m_statusLabel->setText("Saved: " + name);
    }
}

void MainWindow::onLoadProject() {
    QString path = QFileDialog::getOpenFileName(this, "Project kholo (local)", AppPaths::projects(), "ReelForge project (*.json)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) { QMessageBox::warning(this, "ReelForge", "Project file kharab hai."); return; }
    m_project = doc.object();
    m_currentProjectName = QFileInfo(path).fileName();
    m_selectedClip = -1;
    refreshTimelineList();
}

// ---------------------------------------------------------------- export
void MainWindow::startRender(bool fast, const QString& outPath) {
    if (m_ffmpeg.isEmpty()) { QMessageBox::critical(this, "ReelForge", "ffmpeg nahi mila."); return; }
    QJsonArray clips = J::arr(m_project, "clips");
    if (clips.isEmpty()) { QMessageBox::information(this, "ReelForge", "Pehle timeline me photos daalo."); return; }

    m_progressDlg = new QProgressDialog(fast ? "Preview ban raha hai..." : "Export ho raha hai...", "Cancel", 0, 100, this);
    m_progressDlg->setWindowModality(Qt::ApplicationModal);
    m_progressDlg->setMinimumDuration(0);
    connect(m_progressDlg, &QProgressDialog::canceled, this, &MainWindow::onCancelExport);
    m_progressDlg->show();

    auto* engine = new RenderEngine(m_ffmpeg, m_ffprobe, m_project, outPath, AppPaths::media(),
        fast, m_font, m_emojiFont, Catalogs::cpuCount());
    m_renderEngine = engine;

    std::thread([this, engine, outPath]() {
        bool ok = true;
        QString message, file;
        try {
            auto result = engine->run([this](double p) {
                QMetaObject::invokeMethod(this, "onRenderProgress", Qt::QueuedConnection, Q_ARG(double, p));
            });
            file = result.file;
            message = QString("%1s me ban gaya, %2 MB").arg(result.seconds, 0, 'f', 1).arg(result.size / 1048576.0, 0, 'f', 1);
        } catch (const CancelledException&) {
            ok = false; message = "Cancel kar diya";
        } catch (const std::exception& e) {
            ok = false; message = QString::fromStdString(e.what());
        }
        QMetaObject::invokeMethod(this, "onRenderFinished", Qt::QueuedConnection,
            Q_ARG(bool, ok), Q_ARG(QString, message), Q_ARG(QString, file));
    }).detach();
}

void MainWindow::onExport() {
    QString stamp = "reelforge-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ".mp4";
    startRender(false, QDir(AppPaths::exports()).filePath(stamp));
}

void MainWindow::onCancelExport() {
    if (m_renderEngine) m_renderEngine->cancel();
}

void MainWindow::onRenderProgress(double p) {
    if (m_progressDlg) m_progressDlg->setValue((int)(p * 100));
}

void MainWindow::onRenderFinished(bool ok, const QString& message, const QString& outFile) {
    if (m_progressDlg) { m_progressDlg->close(); m_progressDlg->deleteLater(); m_progressDlg = nullptr; }
    delete m_renderEngine;
    m_renderEngine = nullptr;

    if (ok) {
        m_statusLabel->setText("Ho gaya: " + message);
        if (!outFile.isEmpty() && outFile.contains(AppPaths::exports())) {
            auto reply = QMessageBox::question(this, "ReelForge", "Export ho gaya (" + message + "). Folder kholein?");
            if (reply == QMessageBox::Yes)
                QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(outFile).absolutePath()));
        }
    } else {
        m_statusLabel->setText("Rukk gaya: " + message);
        if (message != "Cancel kar diya")
            QMessageBox::warning(this, "ReelForge", message);
    }
}

} // namespace rf
