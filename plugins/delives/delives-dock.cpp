extern "C" {
#include <libsecret/secret.h>
}

#include "delives-dock.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <obs.h>

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QPlainTextEdit>
#include <QTimer>
#include <QMessageBox>
#include <QToolTip>
#include <QClipboard>
#include <QApplication>
#include <QUrl>
#include <QUrlQuery>

static const SecretSchema DELIVES_SCHEMA = {
	"io.github.devini9.delives",
	SECRET_SCHEMA_NONE,
	{
		{"youtube-stream-key", SECRET_SCHEMA_ATTRIBUTE_STRING},
		{NULL, (SecretSchemaAttributeType)0},
	},
};

static QWidget *delives_dock_widget = nullptr;

static QLineEdit *keyEdit = nullptr;
static QLabel *keyStatusLabel = nullptr;
static QPushButton *saveKeyBtn = nullptr;
static QRadioButton *preset1080p = nullptr;
static QRadioButton *preset1440p = nullptr;
static QRadioButton *preset4kp = nullptr;
static QPushButton *applyPresetBtn = nullptr;
static QPushButton *testLiveBtn = nullptr;
static QPlainTextEdit *testResultEdit = nullptr;
static QLabel *rtmpLabel = nullptr;

static QLineEdit *ytChatUrlEdit = nullptr;
static QPushButton *ytChatConnectBtn = nullptr;

static QTimer *testTimer = nullptr;

static void update_button_states()
{
	bool hasKey = false;
	if (keyStatusLabel) {
		hasKey = (keyStatusLabel->text() == QString("saved"));
	}
	if (applyPresetBtn)
		applyPresetBtn->setEnabled(hasKey);
	if (testLiveBtn)
		testLiveBtn->setEnabled(hasKey);
	if (!hasKey) {
		if (applyPresetBtn)
			applyPresetBtn->setToolTip(QString("Save stream key first"));
		if (testLiveBtn)
			testLiveBtn->setToolTip(QString("Save stream key first"));
	} else {
		if (applyPresetBtn)
			applyPresetBtn->setToolTip(QString(""));
		if (testLiveBtn)
			testLiveBtn->setToolTip(QString(""));
	}
}

static void check_saved_key()
{
	GError *error = nullptr;
	gchar *password = secret_password_lookup_sync(&DELIVES_SCHEMA,
						     nullptr, &error,
						     "youtube-stream-key",
						     "true",
						     NULL);
	if (error) {
		g_error_free(error);
		error = nullptr;
	}
	if (password && strlen(password) > 0) {
		keyStatusLabel->setText(QString("saved"));
		secret_password_free(password);
	} else {
		if (password)
			secret_password_free(password);
		keyStatusLabel->setText(QString("not saved"));
	}
	update_button_states();
}

static void save_key_clicked()
{
	QString key = keyEdit->text();
	if (key.isEmpty()) {
		return;
	}
	GError *error = nullptr;
	gboolean result = secret_password_store_sync(&DELIVES_SCHEMA,
						    SECRET_COLLECTION_DEFAULT,
						    "deLives YouTube Stream Key",
						    key.toUtf8().constData(),
						    nullptr, &error,
						    "youtube-stream-key",
						    "true",
						    NULL);
	if (error) {
		g_error_free(error);
		keyStatusLabel->setText(QString("not saved"));
	} else {
		if (result)
			keyStatusLabel->setText(QString("saved"));
		else
			keyStatusLabel->setText(QString("not saved"));
	}
	keyEdit->clear();
	update_button_states();
}

static void apply_preset_clicked()
{
	config_t *global_config = obs_frontend_get_profile_config();
	if (!global_config)
		return;

	if (preset1080p && preset1080p->isChecked()) {
		config_set_string(global_config, "Video", "OutputCX", "1920");
		config_set_string(global_config, "Video", "OutputCY", "1080");
		config_set_string(global_config, "Video", "BaseCX", "1920");
		config_set_string(global_config, "Video", "BaseCY", "1080");
		config_set_uint(global_config, "Video", "FPSInt", 60);
		config_set_string(global_config, "SimpleOutput", "VBitrate", "6000");
		config_set_string(global_config, "SimpleOutput", "StreamEncoder", "x264");
		config_set_string(global_config, "AdvOut", "VBitrate", "6000");
		config_set_string(global_config, "AdvOut", "Encoder", "obs_x264");
	} else if (preset1440p && preset1440p->isChecked()) {
		config_set_string(global_config, "Video", "OutputCX", "2560");
		config_set_string(global_config, "Video", "OutputCY", "1440");
		config_set_string(global_config, "Video", "BaseCX", "2560");
		config_set_string(global_config, "Video", "BaseCY", "1440");
		config_set_uint(global_config, "Video", "FPSInt", 60);
		config_set_string(global_config, "SimpleOutput", "VBitrate", "9000");
		config_set_string(global_config, "SimpleOutput", "StreamEncoder", "x264");
		config_set_string(global_config, "AdvOut", "VBitrate", "9000");
		config_set_string(global_config, "AdvOut", "Encoder", "obs_x264");
	} else if (preset4kp && preset4kp->isChecked()) {
		config_set_string(global_config, "Video", "OutputCX", "3840");
		config_set_string(global_config, "Video", "OutputCY", "2160");
		config_set_string(global_config, "Video", "BaseCX", "3840");
		config_set_string(global_config, "Video", "BaseCY", "2160");
		config_set_uint(global_config, "Video", "FPSInt", 60);
		config_set_string(global_config, "SimpleOutput", "VBitrate", "20000");
		config_set_string(global_config, "SimpleOutput", "StreamEncoder", "x264");
		config_set_string(global_config, "AdvOut", "VBitrate", "20000");
		config_set_string(global_config, "AdvOut", "Encoder", "obs_x264");
	}

	config_save(global_config);
	QMessageBox::information(delives_dock_widget, "deLives", 
		"Preset applied. Reopen Settings to reflect changes.");
}

static void test_live_finished()
{
	if (testTimer) {
		testTimer->stop();
		testTimer->deleteLater();
		testTimer = nullptr;
	}

	obs_frontend_streaming_stop();

	obs_output_t *output = obs_frontend_get_streaming_output();
	uint64_t total_bytes = 0;
	uint64_t frames_dropped = 0;
	uint64_t total_frames = 0;
	double bitrate_kbps = 0.0;

	if (output) {
		total_bytes = obs_output_get_total_bytes(output);
		frames_dropped = obs_output_get_frames_dropped(output);
		total_frames = obs_output_get_total_frames(output);
		if (total_frames > 0) {
			bitrate_kbps = (total_bytes * 8.0) / 60.0 / 1000.0;
		}
		obs_output_release(output);
	}

	QString result;
	result += QString("Duration: 60s\n");
	result += QString("Bytes sent: %1\n").arg(total_bytes);
	result += QString("Average bitrate: %1 kbps\n").arg(QString::number(bitrate_kbps, 'f', 2));
	double drop_pct = 0.0;
	if (total_frames > 0) {
		drop_pct = (frames_dropped * 100.0) / total_frames;
	}
	result += QString("Frames dropped: %1 (%2%)\n").arg(frames_dropped).arg(QString::number(drop_pct, 'f', 2));
	
	if (drop_pct > 1.0 || bitrate_kbps < 4800.0) {
		result += QString("Verdict: atenção\n");
	} else {
		result += QString("Verdict: OK\n");
	}

	testResultEdit->setPlainText(result);
	if (testLiveBtn)
		testLiveBtn->setEnabled(true);
}

static void test_live_clicked()
{
	if (!keyStatusLabel || keyStatusLabel->text() != QString("saved")) {
		return;
	}

	GError *error = nullptr;
	gchar *password = secret_password_lookup_sync(&DELIVES_SCHEMA,
						     nullptr, &error,
						     "youtube-stream-key",
						     "true",
						     NULL);
	if (error) {
		g_error_free(error);
		return;
	}
	if (!password || strlen(password) == 0) {
		if (password)
			secret_password_free(password);
		return;
	}

	obs_service_t *service = obs_frontend_get_streaming_service();
	if (!service) {
		secret_password_free(password);
		return;
	}

	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "server", "rtmp://a.rtmp.youtube.com/live2");
	obs_data_set_string(settings, "key", password);
	obs_service_update(service, settings);
	obs_data_release(settings);
	secret_password_free(password);

	obs_frontend_save_streaming_service();

	if (testLiveBtn)
		testLiveBtn->setEnabled(false);
	testResultEdit->clear();

	obs_frontend_streaming_start();

	testTimer = new QTimer(delives_dock_widget);
	testTimer->setSingleShot(true);
	QObject::connect(testTimer, &QTimer::timeout, test_live_finished);
	testTimer->start(60000);
}

static void connect_yt_chat_clicked()
{
	if (!ytChatUrlEdit)
		return;

	QString urlText = ytChatUrlEdit->text().trimmed();
	if (urlText.isEmpty())
		return;

	QString videoId;
	QUrl url(urlText);
	if (url.isValid()) {
		if (url.host().contains("youtube.com")) {
			if (url.path().contains("/watch")) {
				QUrlQuery query(url);
				videoId = query.queryItemValue("v");
			} else if (url.path().startsWith("/live/")) {
				videoId = url.path().split('/').last().split('?').first();
			} else if (url.path().contains("/live_chat")) {
				QUrlQuery query(url);
				videoId = query.queryItemValue("v");
			}
		} else if (url.host().contains("youtu.be")) {
			videoId = url.path().remove('/').split('?').first();
		}
	}

	if (videoId.isEmpty()) {
		QMessageBox::information(delives_dock_widget, "deLives",
					"Não foi possível extrair o ID do vídeo/live da URL fornecida.");
		return;
	}

	QString chatUrl = QString("https://www.youtube.com/live_chat?v=%1").arg(videoId);

	QClipboard *clipboard = QApplication::clipboard();
	if (clipboard) {
		clipboard->setText(chatUrl);
	}

	QMessageBox::information(delives_dock_widget, "deLives",
					QString("URL do chat copiada para a área de transferência:\n%1\n\n"
						"Passos:\n"
						"1) Docks → Adicionar novo dock de navegador\n"
						"2) Colar a URL copiada\n"
						"3) Definir título \"Chat da live\"").arg(chatUrl));
}

static void setup_dock()
{
	delives_dock_widget = new QWidget();
	QVBoxLayout *layout = new QVBoxLayout(delives_dock_widget);

	// Stream key section
	QGroupBox *keyGroup = new QGroupBox("YouTube Stream Key");
	QVBoxLayout *keyLayout = new QVBoxLayout(keyGroup);
	QHBoxLayout *keyInputLayout = new QHBoxLayout();
	keyEdit = new QLineEdit();
	keyEdit->setEchoMode(QLineEdit::Password);
	keyEdit->setPlaceholderText("Chave de stream YouTube");
	saveKeyBtn = new QPushButton("Salvar");
	keyInputLayout->addWidget(keyEdit);
	keyInputLayout->addWidget(saveKeyBtn);
	keyStatusLabel = new QLabel("not saved");
	keyLayout->addLayout(keyInputLayout);
	keyLayout->addWidget(keyStatusLabel);
	QObject::connect(saveKeyBtn, &QPushButton::clicked, save_key_clicked);
	layout->addWidget(keyGroup);

	// Presets section
	QGroupBox *presetGroup = new QGroupBox("High Resolution Presets");
	QVBoxLayout *presetLayout = new QVBoxLayout(presetGroup);
	preset1080p = new QRadioButton("1080p60 (6000 kbps)");
	preset1440p = new QRadioButton("1440p60 (9000 kbps)");
	preset4kp = new QRadioButton("4K60 (20000 kbps)");
	preset1080p->setChecked(true);
	QButtonGroup *presetGroupBtns = new QButtonGroup(presetGroup);
	presetGroupBtns->addButton(preset1080p);
	presetGroupBtns->addButton(preset1440p);
	presetGroupBtns->addButton(preset4kp);
	presetLayout->addWidget(preset1080p);
	presetLayout->addWidget(preset1440p);
	presetLayout->addWidget(preset4kp);
	applyPresetBtn = new QPushButton("Aplicar");
	QObject::connect(applyPresetBtn, &QPushButton::clicked, apply_preset_clicked);
	presetLayout->addWidget(applyPresetBtn);
	layout->addWidget(presetGroup);

	// Test live section
	QGroupBox *testGroup = new QGroupBox("Live Test");
	QVBoxLayout *testLayout = new QVBoxLayout(testGroup);
	testLiveBtn = new QPushButton("Teste de live (60s)");
	QObject::connect(testLiveBtn, &QPushButton::clicked, test_live_clicked);
	testLayout->addWidget(testLiveBtn);
	testResultEdit = new QPlainTextEdit();
	testResultEdit->setReadOnly(true);
	testResultEdit->setMaximumHeight(100);
	testLayout->addWidget(testResultEdit);
	layout->addWidget(testGroup);

	// YouTube Chat section
	QGroupBox *chatGroup = new QGroupBox(QString::fromUtf8("💬 Chat do YouTube"));
	QVBoxLayout *chatLayout = new QVBoxLayout(chatGroup);
	ytChatUrlEdit = new QLineEdit();
	ytChatUrlEdit->setPlaceholderText("URL do vídeo/live");
	ytChatConnectBtn = new QPushButton("Conectar chat");
	QObject::connect(ytChatConnectBtn, &QPushButton::clicked, connect_yt_chat_clicked);
	chatLayout->addWidget(ytChatUrlEdit);
	chatLayout->addWidget(ytChatConnectBtn);
	layout->addWidget(chatGroup);

	// Game capture section
	QGroupBox *gameCaptureGroup = new QGroupBox("Game Capture");
	QVBoxLayout *gameCaptureLayout = new QVBoxLayout(gameCaptureGroup);
	QPushButton *captureGameBtn = new QPushButton(QString::fromUtf8("🎮 Capturar Jogo"));
	QLabel *gameCaptureHint = new QLabel("Para jogos via Proton/Steam, use o modo janela ou gamescope para captura limpa");
	gameCaptureHint->setWordWrap(true);
	gameCaptureHint->setStyleSheet("color: gray;");
	QObject::connect(captureGameBtn, &QPushButton::clicked, []() {
		obs_source_t *source = obs_source_create("pipewire-window-capture-source",
							 "Capturar Jogo", nullptr, nullptr);
		if (source) {
			obs_scene_t *scene = obs_scene_from_source(obs_frontend_get_current_scene());
			if (scene) {
				obs_scene_add(scene, source);
			}
			obs_frontend_open_source_properties(source);
			obs_source_release(source);
		}
	});
	gameCaptureLayout->addWidget(captureGameBtn);
	gameCaptureLayout->addWidget(gameCaptureHint);
	layout->addWidget(gameCaptureGroup);

	// RTMP label
	rtmpLabel = new QLabel("rtmp://a.rtmp.youtube.com/live2");
	rtmpLabel->setStyleSheet("color: gray;");
	layout->addWidget(rtmpLabel);

	layout->addStretch();

	check_saved_key();
}

void create_delives_dock(void)
{
	setup_dock();
	obs_frontend_add_dock_by_id("deLives", "deLives", delives_dock_widget);
}

void destroy_delives_dock(void)
{
	if (testTimer) {
		testTimer->stop();
		testTimer->deleteLater();
		testTimer = nullptr;
	}
	if (delives_dock_widget) {
		delives_dock_widget->deleteLater();
		delives_dock_widget = nullptr;
	}
}
