#pragma once
#include <QMainWindow>
#include <QJsonObject>
#include <memory>

class QListWidget;
class QListWidgetItem;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QLabel;
class QProgressDialog;
class QThread;

namespace rf { class MediaStore; class RenderEngine; }

namespace rf {

/// Poora app ek hi native window hai -- koi browser tab, koi WebView, koi
/// localhost server nahi. Media seedhe local disk se load hoti hai
/// (QPixmap/QImage), export seedhe ffmpeg subprocess (QProcess) se hota hai.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onImportMedia();
    void onAddToTimeline();
    void onMediaSelected(QListWidgetItem* item);
    void onClipSelected(QListWidgetItem* item);
    void onClipPropsChanged();
    void onMoveUp();
    void onMoveDown();
    void onRemoveClip();
    void onSaveProject();
    void onNewProject();
    void onLoadProject();
    void onExport();
    void onCancelExport();
    void onRenderProgress(double p);
    void onRenderFinished(bool ok, const QString& message, const QString& outFile);

private:
    void setupUi();
    void detectTools();
    void refreshMediaList();
    void refreshTimelineList();
    void refreshPreview();
    void loadClipIntoProps(int idx);
    QJsonObject currentProject() const;
    void startRender(bool fast, const QString& outPath);

    QListWidget* m_mediaList = nullptr;
    QListWidget* m_timelineList = nullptr;
    QComboBox* m_motionBox = nullptr;
    QComboBox* m_transBox = nullptr;
    QComboBox* m_lookBox = nullptr;
    QDoubleSpinBox* m_durSpin = nullptr;
    QLineEdit* m_captionEdit = nullptr;
    QLabel* m_previewLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QProgressDialog* m_progressDlg = nullptr;

    QString m_ffmpeg, m_ffprobe, m_font, m_emojiFont;
    std::unique_ptr<MediaStore> m_media;
    QJsonObject m_project;
    int m_selectedClip = -1;
    QString m_currentProjectName;

    QThread* m_renderThread = nullptr;
    RenderEngine* m_renderEngine = nullptr; // owned by worker, guarded for cancel
};

} // namespace rf
