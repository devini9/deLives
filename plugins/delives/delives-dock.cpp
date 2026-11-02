extern "C" {
#include <libsecret/secret.h>
#include <obs.h>
#include <obs-encoder.h>
#include <obs-service.h>
}

#include "delives-dock.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <obs.h>
#include <graphics/vec2.h>

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
#include <QAction>
#include <QMainWindow>
#include <QMenuBar>
#include <QComboBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QCheckBox>
#include <string>
#include <vector>

static const SecretSchema DELIVES_SCHEMA = {
	"io.github.devini9.delives",
	SECRET_SCHEMA_NONE,
	{
		{"stream-key", SECRET_SCHEMA_ATTRIBUTE_STRING},
		{"platform", SECRET_SCHEMA_ATTRIBUTE_STRING},
		{NULL, (SecretSchemaAttributeType)0},
	},
};

static QWidget *delives_dock_widget = nullptr;

static QComboBox *platformCombo = nullptr;
static QLineEdit *keyEdit = nullptr;
static QLabel *keyStatusLabel = nullptr;
static QPushButton *saveKeyBtn = nullptr;
static QLabel *platformStatusLabel = nullptr;
static QRadioButton *preset1080p = nullptr;
static QRadioButton *preset1440p = nullptr;
static QRadioButton *preset4kp = nullptr;
static QPushButton *applyPresetBtn = nullptr;
static QPushButton *testLiveBtn = nullptr;
static QPlainTextEdit *testResultEdit = nullptr;
static QLabel *rtmpLabel = nullptr;

static QLineEdit *ytChatUrlEdit = nullptr;
static QPushButton *ytChatConnectBtn = nullptr;
static QPushButton *chatOverlayBtn = nullptr;

// Multi-stream
static QCheckBox *multiYtCheck = nullptr;
static QCheckBox *multiKickCheck = nullptr;
static QCheckBox *multiTwitchCheck = nullptr;

// Multi-chat
static QLineEdit *twitchChatChannelEdit = nullptr;
static QPushButton *twitchChatOverlayBtn = nullptr;
static QLineEdit *kickChatUrlEdit = nullptr;
static QPushButton *kickChatOverlayBtn = nullptr;

static QTimer *testTimer = nullptr;

// Track multistream outputs
static std::vector<obs_output_t *> multistream_outputs;
static bool multistream_active = false;

static void update_button_states()
{
	bool hasKey = false;
	if (keyStatusLabel) {
		hasKey = (keyStatusLabel->text() == QString("salvo") || keyStatusLabel->text() == QString("saved"));
	}
	if (applyPresetBtn)
		applyPresetBtn->setEnabled(hasKey);
	if (testLiveBtn)
		testLiveBtn->setEnabled(hasKey);
	if (!hasKey) {
		if (applyPresetBtn)
			applyPresetBtn->setToolTip(QString("Salve a chave da stream primeiro"));
		if (testLiveBtn)
			testLiveBtn->setToolTip(QString("Salve a chave da stream primeiro"));
	} else {
		if (applyPresetBtn)
			applyPresetBtn->setToolTip(QString(""));
		if (testLiveBtn)
			testLiveBtn->setToolTip(QString(""));
	}
}

static QString get_current_platform()
{
	if (platformCombo) {
		return platformCombo->currentData().toString();
	}
	return "youtube";
}

static QString get_rtmp_url_for_platform(const QString &platform)
{
	if (platform == "kick")
		return "rtmp://fa723fc1a1719.global-contribute.live-video.net";
	if (platform == "twitch")
		return "rtmp://live.twitch.tv/app";
	return "rtmp://a.rtmp.youtube.com/live2";
}

static void update_rtmp_display()
{
	QString platform = get_current_platform();
	QString rtmp = get_rtmp_url_for_platform(platform);
	if (rtmpLabel)
		rtmpLabel->setText(rtmp);
}

static void check_saved_key();

static void platform_changed(int)
{
	update_rtmp_display();
	check_saved_key();
}

static void check_saved_key()
{
	QString platform = get_current_platform();
	GError *error = nullptr;
	gchar *password = secret_password_lookup_sync(&DELIVES_SCHEMA,
						     nullptr, &error,
						     "stream-key",
						     "true",
						     "platform", platform.toUtf8().constData(),
						     NULL);
	if (error) {
		g_error_free(error);
		error = nullptr;
	}
	if (password && strlen(password) > 0) {
		if (keyStatusLabel)
			keyStatusLabel->setText(QString("salvo"));
		if (platformStatusLabel) {
			if (platform == "youtube")
				platformStatusLabel->setText(QString("Plataforma ativa: YouTube"));
			else if (platform == "kick")
				platformStatusLabel->setText(QString("Plataforma ativa: Kick"));
			else if (platform == "twitch")
				platformStatusLabel->setText(QString("Plataforma ativa: Twitch"));
			else
				platformStatusLabel->setText(QString("Plataforma ativa"));
		}
		secret_password_free(password);
	} else {
		if (password)
			secret_password_free(password);
		if (keyStatusLabel)
			keyStatusLabel->setText(QString("não salvo"));
		if (platformStatusLabel) {
			if (platform == "youtube")
				platformStatusLabel->setText(QString("Plataforma: YouTube"));
			else if (platform == "kick")
				platformStatusLabel->setText(QString("Plataforma: Kick"));
			else if (platform == "twitch")
				platformStatusLabel->setText(QString("Plataforma: Twitch"));
			else
				platformStatusLabel->setText(QString("Plataforma"));
		}
	}
	update_button_states();
}

static void save_key_clicked()
{
	if (!keyEdit)
		return;
	QString key = keyEdit->text();
	if (key.isEmpty()) {
		return;
	}
	QString platform = get_current_platform();
	QString label;
	if (platform == "youtube")
		label = "deLives YouTube Stream Key";
	else if (platform == "kick")
		label = "deLives Kick Stream Key";
	else if (platform == "twitch")
		label = "deLives Twitch Stream Key";
	else
		label = "deLives Stream Key";
	GError *error = nullptr;
	gboolean result = secret_password_store_sync(&DELIVES_SCHEMA,
						    SECRET_COLLECTION_DEFAULT,
						    label.toUtf8().constData(),
						    key.toUtf8().constData(),
						    nullptr, &error,
						    "stream-key",
						    "true",
						    "platform", platform.toUtf8().constData(),
						    NULL);
	if (error) {
		g_error_free(error);
		if (keyStatusLabel)
			keyStatusLabel->setText(QString("não salvo"));
	} else {
		if (result && keyStatusLabel)
			keyStatusLabel->setText(QString("salvo"));
		else if (keyStatusLabel)
			keyStatusLabel->setText(QString("não salvo"));
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

static void stop_multistream()
{
	if (multistream_active) {
		for (auto *out : multistream_outputs) {
			if (out && obs_output_active(out)) {
				obs_output_stop(out);
			}
			if (out) {
				obs_output_release(out);
			}
		}
		multistream_outputs.clear();
		multistream_active = false;
	}
}

static void start_multistream()
{
	if (multistream_active) {
		stop_multistream();
	}

	// Get encoders from current streaming output if available
	obs_output_t *main_output = obs_frontend_get_streaming_output();
	obs_encoder_t *venc = nullptr;
	obs_encoder_t *aenc = nullptr;
	if (main_output) {
		venc = obs_output_get_video_encoder(main_output);
		aenc = obs_output_get_audio_encoder(main_output, 0);
		obs_output_release(main_output);
	}

	if (!venc) {
		QMessageBox::information(delives_dock_widget, "deLives",
					"Não foi possível obter o encoder de vídeo atual. Configure a transmissão normalmente primeiro ou use o encoder ativo.");
		return;
	}

	struct PlatformInfo {
		QString key;
		bool checked;
	};
	std::vector<PlatformInfo> platforms;
	if (multiYtCheck && multiYtCheck->isChecked())
		platforms.push_back({"youtube", true});
	if (multiKickCheck && multiKickCheck->isChecked())
		platforms.push_back({"kick", true});
	if (multiTwitchCheck && multiTwitchCheck->isChecked())
		platforms.push_back({"twitch", true});

	if (platforms.empty()) {
		QMessageBox::information(delives_dock_widget, "deLives",
					"Selecione pelo menos uma plataforma para multistream.");
		return;
	}

	for (const auto &p : platforms) {
		GError *error = nullptr;
		gchar *password = secret_password_lookup_sync(&DELIVES_SCHEMA,
							     nullptr, &error,
							     "stream-key",
							     "true",
							     "platform", p.key.toUtf8().constData(),
							     NULL);
		if (error) {
			g_error_free(error);
			error = nullptr;
		}
		if (!password || strlen(password) == 0) {
			if (password)
				secret_password_free(password);
			QString name = p.key == "youtube" ? "YouTube" : (p.key == "kick" ? "Kick" : "Twitch");
			QMessageBox::information(delives_dock_widget, "deLives",
						QString("Salve a chave da stream para %1 antes do multistream.").arg(name));
			stop_multistream();
			return;
		}

		obs_data_t *service_settings = obs_data_create();
		obs_data_set_string(service_settings, "server", get_rtmp_url_for_platform(p.key).toUtf8().constData());
		obs_data_set_string(service_settings, "key", password);
		obs_service_t *service = obs_service_create("rtmp_custom", "delives_multistream_service", service_settings, nullptr);
		obs_data_release(service_settings);
		secret_password_free(password);

		if (!service) {
			stop_multistream();
			return;
		}

		obs_output_t *output = obs_output_create("rtmp_output", QString("delives_multistream_%1").arg(p.key).toUtf8().constData(), nullptr, nullptr);
		if (!output) {
			obs_service_release(service);
			stop_multistream();
			return;
		}

		obs_output_set_service(output, service);
		obs_output_set_video_encoder(output, venc);
		if (aenc) {
			obs_output_set_audio_encoder(output, aenc, 0);
		}
		obs_output_set_media(output, obs_get_video(), obs_get_audio());

		if (!obs_output_start(output)) {
			obs_output_release(output);
			obs_service_release(service);
			stop_multistream();
			return;
		}

		multistream_outputs.push_back(output);
		obs_service_release(service);
	}

	multistream_active = true;
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
	if (!keyStatusLabel || !(keyStatusLabel->text() == QString("salvo") || keyStatusLabel->text() == QString("saved"))) {
		return;
	}

	QString platform = get_current_platform();
	GError *error = nullptr;
	gchar *password = secret_password_lookup_sync(&DELIVES_SCHEMA,
						     nullptr, &error,
						     "stream-key",
						     "true",
						     "platform", platform.toUtf8().constData(),
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
	obs_data_set_string(settings, "server", get_rtmp_url_for_platform(platform).toUtf8().constData());
	obs_data_set_string(settings, "key", password);
	obs_service_update(service, settings);
	obs_data_release(settings);
	secret_password_free(password);

	obs_frontend_save_streaming_service();

	if (testLiveBtn)
		testLiveBtn->setEnabled(false);
	if (testResultEdit)
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

static void create_source_quick(const char *id, const char *name, bool open_props)
{
	obs_source_t *source = obs_source_create(id, name, nullptr, nullptr);
	if (source) {
		obs_scene_t *scene = obs_scene_from_source(obs_frontend_get_current_scene());
		if (scene) {
			obs_scene_add(scene, source);
		}
		if (open_props)
			obs_frontend_open_source_properties(source);
		obs_source_release(source);
	}
}

static void create_chat_overlay_clicked()
{
	QString urlText;
	if (ytChatUrlEdit)
		urlText = ytChatUrlEdit->text().trimmed();

	QString videoId;
	if (!urlText.isEmpty()) {
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
	}

	QString chatUrl;
	if (!videoId.isEmpty())
		chatUrl = QString("https://www.youtube.com/live_chat?v=%1&embed_domain=localhost").arg(videoId);
	else
		chatUrl = QString("https://www.youtube.com/live_chat");

	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "url", chatUrl.toUtf8().constData());
	obs_data_set_bool(settings, "is_local_file", false);
	obs_source_t *source = obs_source_create("browser_source", "Chat da live", settings, nullptr);
	obs_data_release(settings);
	if (source) {
		obs_scene_t *scene = obs_scene_from_source(obs_frontend_get_current_scene());
		if (scene) {
			obs_sceneitem_t *item = obs_scene_add(scene, source);
			if (item) {
				struct vec2 pos = {20.0f, 0.0f};
				obs_sceneitem_set_pos(item, &pos);
				struct vec2 scale = {0.4f, 0.4f};
				obs_sceneitem_set_scale(item, &scale);
				obs_sceneitem_set_alignment(item, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM);
			}
		}
		obs_frontend_open_source_properties(source);
		obs_source_release(source);
	}
}

static void setup_dock()
{
	delives_dock_widget = new QWidget();
	QVBoxLayout *layout = new QVBoxLayout(delives_dock_widget);

	// Platform section
	QGroupBox *platformGroup = new QGroupBox(QString::fromUtf8("Plataforma"));
	QVBoxLayout *platformLayout = new QVBoxLayout(platformGroup);
	platformCombo = new QComboBox();
	platformCombo->addItem(QString::fromUtf8("YouTube"), "youtube");
	platformCombo->addItem(QString::fromUtf8("Kick"), "kick");
	platformCombo->addItem(QString::fromUtf8("Twitch"), "twitch");
	QObject::connect(platformCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), platform_changed);
	platformLayout->addWidget(platformCombo);
	platformStatusLabel = new QLabel(QString::fromUtf8("Plataforma: YouTube"));
	platformLayout->addWidget(platformStatusLabel);
	layout->addWidget(platformGroup);

	// Multistream section
	QGroupBox *multiGroup = new QGroupBox(QString::fromUtf8("Multistream (transmitir em vários ao mesmo tempo)"));
	QVBoxLayout *multiLayout = new QVBoxLayout(multiGroup);
	multiYtCheck = new QCheckBox(QString::fromUtf8("YouTube"));
	multiKickCheck = new QCheckBox(QString::fromUtf8("Kick"));
	multiTwitchCheck = new QCheckBox(QString::fromUtf8("Twitch"));
	multiLayout->addWidget(multiYtCheck);
	multiLayout->addWidget(multiKickCheck);
	multiLayout->addWidget(multiTwitchCheck);
	layout->addWidget(multiGroup);

	// Stream key section
	QGroupBox *keyGroup = new QGroupBox(QString::fromUtf8("Chave da stream"));
	QVBoxLayout *keyLayout = new QVBoxLayout(keyGroup);
	QHBoxLayout *keyInputLayout = new QHBoxLayout();
	keyEdit = new QLineEdit();
	keyEdit->setEchoMode(QLineEdit::Password);
	keyEdit->setPlaceholderText(QString::fromUtf8("Chave da stream"));
	saveKeyBtn = new QPushButton(QString::fromUtf8("Salvar"));
	keyInputLayout->addWidget(keyEdit);
	keyInputLayout->addWidget(saveKeyBtn);
	keyStatusLabel = new QLabel("não salvo");
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

	// Multistream control section
	QGroupBox *multiCtrlGroup = new QGroupBox(QString::fromUtf8("Controle Multistream"));
	QVBoxLayout *multiCtrlLayout = new QVBoxLayout(multiCtrlGroup);
	QPushButton *multiStartBtn = new QPushButton(QString::fromUtf8("Iniciar Multistream"));
	QPushButton *multiStopBtn = new QPushButton(QString::fromUtf8("Parar Multistream"));
	QObject::connect(multiStartBtn, &QPushButton::clicked, start_multistream);
	QObject::connect(multiStopBtn, &QPushButton::clicked, stop_multistream);
	multiCtrlLayout->addWidget(multiStartBtn);
	multiCtrlLayout->addWidget(multiStopBtn);
	layout->addWidget(multiCtrlGroup);

	// Test live section
	QGroupBox *testGroup = new QGroupBox(QString::fromUtf8("Teste de live"));
	QVBoxLayout *testLayout = new QVBoxLayout(testGroup);
	testLiveBtn = new QPushButton(QString::fromUtf8("Teste de live (60s)"));
	QObject::connect(testLiveBtn, &QPushButton::clicked, test_live_clicked);
	testLayout->addWidget(testLiveBtn);
	testResultEdit = new QPlainTextEdit();
	testResultEdit->setReadOnly(true);
	testResultEdit->setMaximumHeight(100);
	testLayout->addWidget(testResultEdit);
	layout->addWidget(testGroup);

	// Chats section
	QGroupBox *chatsGroup = new QGroupBox(QString::fromUtf8("Chats (3 simultâneos)"));
	QVBoxLayout *chatsLayout = new QVBoxLayout(chatsGroup);

	// YouTube
	QGroupBox *ytChatGroup = new QGroupBox(QString::fromUtf8("💬 Chat do YouTube"));
	QVBoxLayout *ytChatLayout = new QVBoxLayout(ytChatGroup);
	ytChatUrlEdit = new QLineEdit();
	ytChatUrlEdit->setPlaceholderText(QString::fromUtf8("URL do vídeo/live"));
	ytChatConnectBtn = new QPushButton(QString::fromUtf8("Conectar chat"));
	QObject::connect(ytChatConnectBtn, &QPushButton::clicked, connect_yt_chat_clicked);
	ytChatLayout->addWidget(ytChatUrlEdit);
	ytChatLayout->addWidget(ytChatConnectBtn);
	chatOverlayBtn = new QPushButton(QString::fromUtf8("💬 Chat na live (YouTube)"));
	QObject::connect(chatOverlayBtn, &QPushButton::clicked, create_chat_overlay_clicked);
	ytChatLayout->addWidget(chatOverlayBtn);
	chatsLayout->addWidget(ytChatGroup);

	// Twitch
	QGroupBox *twitchChatGroup = new QGroupBox(QString::fromUtf8("💬 Chat da Twitch"));
	QVBoxLayout *twitchChatLayout = new QVBoxLayout(twitchChatGroup);
	twitchChatChannelEdit = new QLineEdit();
	twitchChatChannelEdit->setPlaceholderText(QString::fromUtf8("Canal da Twitch (ex: nomecanal)"));
	twitchChatOverlayBtn = new QPushButton(QString::fromUtf8("💬 Chat na live (Twitch)"));
	QObject::connect(twitchChatOverlayBtn, &QPushButton::clicked, []() {
		if (!twitchChatChannelEdit)
			return;
		QString channel = twitchChatChannelEdit->text().trimmed();
		if (channel.isEmpty())
			return;
		if (channel.contains("/")) {
			channel = channel.split('/').last().split('?').first();
		}
		QString chatUrl = QString("https://www.twitch.tv/embed/%1/chat?parent=localhost").arg(channel);
		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, "url", chatUrl.toUtf8().constData());
		obs_data_set_bool(settings, "is_local_file", false);
		obs_source_t *source = obs_source_create("browser_source", "Chat da live - Twitch", settings, nullptr);
		obs_data_release(settings);
		if (source) {
			obs_scene_t *scene = obs_scene_from_source(obs_frontend_get_current_scene());
			if (scene) {
				obs_sceneitem_t *item = obs_scene_add(scene, source);
				if (item) {
					struct vec2 pos = {20.0f, 0.0f};
					obs_sceneitem_set_pos(item, &pos);
					struct vec2 scale = {0.4f, 0.4f};
					obs_sceneitem_set_scale(item, &scale);
					obs_sceneitem_set_alignment(item, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM);
				}
			}
			obs_frontend_open_source_properties(source);
			obs_source_release(source);
		}
	});
	twitchChatLayout->addWidget(twitchChatChannelEdit);
	twitchChatLayout->addWidget(twitchChatOverlayBtn);
	chatsLayout->addWidget(twitchChatGroup);

	// Kick
	QGroupBox *kickChatGroup = new QGroupBox(QString::fromUtf8("💬 Chat da Kick"));
	QVBoxLayout *kickChatLayout = new QVBoxLayout(kickChatGroup);
	kickChatUrlEdit = new QLineEdit();
	kickChatUrlEdit->setPlaceholderText(QString::fromUtf8("Canal da Kick ou URL"));
	kickChatOverlayBtn = new QPushButton(QString::fromUtf8("💬 Chat na live (Kick)"));
	QObject::connect(kickChatOverlayBtn, &QPushButton::clicked, []() {
		if (!kickChatUrlEdit)
			return;
		QString input = kickChatUrlEdit->text().trimmed();
		if (input.isEmpty())
			return;
		QString channel;
		QUrl url(input);
		if (url.isValid() && url.host().contains("kick.com")) {
			QString path = url.path();
			path = path.replace("/", "");
			channel = path.split('?').first().split('#').first();
		} else {
			channel = input.replace("/", "");
		}
		QString chatUrl = QString("https://kick.com/%1/chatroom").arg(channel);
		obs_data_t *settings = obs_data_create();
		obs_data_set_string(settings, "url", chatUrl.toUtf8().constData());
		obs_data_set_bool(settings, "is_local_file", false);
		obs_source_t *source = obs_source_create("browser_source", "Chat da live - Kick", settings, nullptr);
		obs_data_release(settings);
		if (source) {
			obs_scene_t *scene = obs_scene_from_source(obs_frontend_get_current_scene());
			if (scene) {
				obs_sceneitem_t *item = obs_scene_add(scene, source);
				if (item) {
					struct vec2 pos = {20.0f, 0.0f};
					obs_sceneitem_set_pos(item, &pos);
					struct vec2 scale = {0.4f, 0.4f};
					obs_sceneitem_set_scale(item, &scale);
					obs_sceneitem_set_alignment(item, OBS_ALIGN_LEFT | OBS_ALIGN_BOTTOM);
				}
			}
			obs_frontend_open_source_properties(source);
			obs_source_release(source);
		}
	});
	kickChatLayout->addWidget(kickChatUrlEdit);
	kickChatLayout->addWidget(kickChatOverlayBtn);
	chatsLayout->addWidget(kickChatGroup);

	layout->addWidget(chatsGroup);

	// Fast sources section
	QGroupBox *fastSourcesGroup = new QGroupBox(QString::fromUtf8("Fontes rápidas"));
	QVBoxLayout *fastSourcesLayout = new QVBoxLayout(fastSourcesGroup);
	QPushButton *webcamBtn = new QPushButton(QString::fromUtf8("🎥 Webcam"));
	QObject::connect(webcamBtn, &QPushButton::clicked, []() {
		create_source_quick("v4l2_input", "Webcam", true);
	});
	QPushButton *imageBtn = new QPushButton(QString::fromUtf8("🖼️ Imagem"));
	QObject::connect(imageBtn, &QPushButton::clicked, []() {
		QString file = QFileDialog::getOpenFileName(delives_dock_widget, QString::fromUtf8("Selecionar imagem"), QString(), QString::fromUtf8("Imagens (*.png *.jpg *.jpeg *.bmp *.gif)"));
		if (!file.isEmpty()) {
			obs_data_t *settings = obs_data_create();
			obs_data_set_string(settings, "file", file.toUtf8().constData());
			obs_source_t *source = obs_source_create("image_source", "Imagem", settings, nullptr);
			obs_data_release(settings);
			if (source) {
				obs_scene_t *scene = obs_scene_from_source(obs_frontend_get_current_scene());
				if (scene) {
					obs_scene_add(scene, source);
				}
				obs_frontend_open_source_properties(source);
				obs_source_release(source);
			}
		}
	});
	QPushButton *mediaBtn = new QPushButton(QString::fromUtf8("🎬 Mídia"));
	QObject::connect(mediaBtn, &QPushButton::clicked, []() {
		QString file = QFileDialog::getOpenFileName(delives_dock_widget, QString::fromUtf8("Selecionar mídia"), QString(), QString::fromUtf8("Mídia (*.mp4 *.mkv *.webm *.mov *.avi *.mp3 *.ogg)"));
		if (!file.isEmpty()) {
			obs_data_t *settings = obs_data_create();
			obs_data_set_string(settings, "local_file", file.toUtf8().constData());
			obs_source_t *source = obs_source_create("ffmpeg_source", "Mídia", settings, nullptr);
			obs_data_release(settings);
			if (source) {
				obs_scene_t *scene = obs_scene_from_source(obs_frontend_get_current_scene());
				if (scene) {
					obs_scene_add(scene, source);
				}
				obs_frontend_open_source_properties(source);
				obs_source_release(source);
			}
		}
	});
	fastSourcesLayout->addWidget(webcamBtn);
	fastSourcesLayout->addWidget(imageBtn);
	fastSourcesLayout->addWidget(mediaBtn);
	layout->addWidget(fastSourcesGroup);

	// Game capture section
	QGroupBox *gameCaptureGroup = new QGroupBox(QString::fromUtf8("Captura de jogo"));
	QVBoxLayout *gameCaptureLayout = new QVBoxLayout(gameCaptureGroup);
	QPushButton *captureGameBtn = new QPushButton(QString::fromUtf8("🎮 Capturar Jogo"));
	QLabel *gameCaptureHint = new QLabel(QString::fromUtf8("Para jogos via Proton/Steam, use o modo janela ou gamescope para captura limpa"));
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

	// Advanced mode toggle
	QPushButton *advancedModeBtn = new QPushButton(QString::fromUtf8("⚙ Modo avançado"));
	QObject::connect(advancedModeBtn, &QPushButton::clicked, []() {
		config_t *userConfig = obs_frontend_get_user_config();
		if (!userConfig)
			return;
		bool checked = config_get_bool(userConfig, "BasicWindow", "DelivesMode");
		checked = !checked;
		config_set_bool(userConfig, "BasicWindow", "DelivesMode", checked);
		config_save_safe(userConfig, "tmp", nullptr);
		void *mainWindow = obs_frontend_get_main_window();
		if (mainWindow) {
			QMainWindow *mw = static_cast<QMainWindow *>(mainWindow);
			if (mw->menuBar())
				mw->menuBar()->setVisible(checked);
			QAction *action = nullptr;
			// Try to find action by object name through menu actions
			QList<QAction *> actions = mw->findChildren<QAction *>();
			for (QAction *a : actions) {
				if (a->objectName() == "actionToggleDelivesMode") {
					action = a;
					break;
				}
			}
			if (action) {
				action->setChecked(checked);
			}
		}
		QMessageBox::information(delives_dock_widget, "deLives",
					checked ? "Modo avançado ativado (menus completos)." : "Modo avançado desativado.");
	});
	layout->addWidget(advancedModeBtn);

	// RTMP label
	rtmpLabel = new QLabel(get_rtmp_url_for_platform(get_current_platform()));
	rtmpLabel->setStyleSheet("color: gray;");
	layout->addWidget(rtmpLabel);

	layout->addStretch();

	update_rtmp_display();
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
