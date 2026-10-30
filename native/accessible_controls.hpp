#pragma once
#include <QScrollArea>
#include <QFormLayout>
#include <algorithm>

class AccessibleControls : public QWidget {
    QString path,root;
    QJsonObject config;
    QComboBox *applications,*presets;
    QLabel *notice,*peak,*engineState;
    QPushButton *retryRtx;
    QMap<QString,QCheckBox*> toggles;
    QMap<QString,QSpinBox*> values;
    QMap<QString,QComboBox*> choices;
    QTimer debounce;
    QFileSystemWatcher files;
    bool dirty=false,refreshing=false;
    QDBusInterface effect{"org.kde.KWin","/TrueHDR","org.truehdr.Desktop"};
    void save(){
        debounce.stop();if(!dirty)return;
        QString error;
        if(profiles::Store(path).saveCurrent(config,error)){dirty=false;notice->setText("Settings saved.");}
        else notice->setText(error);
    }
    void changed(){if(refreshing)return;dirty=true;debounce.start(180);}
    void refresh(){
        if(dirty)return;auto loaded=updateRead(path);
        if(!profiles::valid(profiles::picture(loaded))){notice->setText("Saved settings could not be read. The existing file has been preserved.");return;}
        refreshing=true;config=loaded;
        const auto& defaults=profiles::pictureDefaults();
        for(auto it=toggles.begin();it!=toggles.end();++it){
            bool master=it.key()=="hdr_enabled"||it.key()=="sharpen_enabled";
            it.value()->setChecked(master?config["enabled"].toBool(true)&&config[it.key()].toBool(true):config[it.key()].toBool(it.key()=="debanding"));
        }
        for(auto it=values.begin();it!=values.end();++it)it.value()->setValue(config[it.key()].toInt(it.key()=="middle_gray"?50:defaults[it.key()].toInt()));
        for(auto it=choices.begin();it!=choices.end();++it){int index=it.value()->findData(config[it.key()].toString(defaults[it.key()].toString()));it.value()->setCurrentIndex(std::max(index,0));}
        QString error;presets->clear();presets->addItems(profiles::Store(path).names(config,error));presets->setCurrentText(profiles::active(config));
        auto state=updateRead(root+"/output/desktop/live-status.json");
        peak->setText(state["display_hdr"].toBool()?QString("%1 nits — %2").arg(state["display_peak"].toInt()).arg(state["display"].toString()):"HDR is unavailable on the selected screen. Sharpening is available.");
        engineState->setText(state["rtx_active"].toBool()?"Using RTX HDR.":state["rtx_fallback"].toString().isEmpty()?"The engine starts when the selected window is processed.":"Using the shader engine: "+state["rtx_fallback"].toString());
        retryRtx->setVisible(!state["rtx_fallback"].toString().isEmpty());
        if(!error.isEmpty())notice->setText(error);refreshing=false;
    }
    void refreshApplications(){
        auto selected=applications->currentData();applications->clear();
        QDBusReply<QString> reply=effect.call("applications");
        for(auto value:QJsonDocument::fromJson(reply.value().toUtf8()).array()){
            auto window=value.toObject();applications->addItem(window["title"].toString(),window["id"].toString());
        }
        int index=applications->findData(selected);if(index>=0)applications->setCurrentIndex(index);
    }
public:
    explicit AccessibleControls(QString profile):path(std::move(profile)),root(QFileInfo(path).absolutePath()){
        setWindowTitle("Chaos HDR — Accessible controls");resize(500,720);effect.setTimeout(1000);
        auto outer=new QVBoxLayout(this);auto scroll=new QScrollArea;scroll->setWidgetResizable(true);outer->addWidget(scroll);
        auto body=new QWidget;scroll->setWidget(body);auto layout=new QVBoxLayout(body);
        auto heading=new QLabel("Chaos HDR");auto font=heading->font();font.setPointSize(font.pointSize()+8);font.setBold(true);heading->setFont(font);layout->addWidget(heading);
        auto form=new QFormLayout;layout->addLayout(form);
        applications=new QComboBox;form->addRow("&Application",applications);
        auto actions=new QHBoxLayout;auto reload=new QPushButton("Refresh applications"),allow=new QPushButton("Allow"),remove=new QPushButton("Remove");
        for(auto* button:{reload,allow,remove})actions->addWidget(button);layout->addLayout(actions);
        connect(reload,&QPushButton::clicked,this,[this]{refreshApplications();});
        auto permission=[this](bool allowed){QDBusReply<bool> reply=effect.call("setApplicationAllowed",applications->currentData().toString(),allowed);notice->setText(reply.isValid()&&reply.value()?(allowed?"Application allowed.":"Application removed."):"The application is no longer available. Refresh the list.");};
        connect(allow,&QPushButton::clicked,this,[permission]{permission(true);});connect(remove,&QPushButton::clicked,this,[permission]{permission(false);});
        presets=new QComboBox;form->addRow("&Profile",presets);
        auto profileActions=new QHBoxLayout;auto create=new QPushButton("Save as…"),erase=new QPushButton("Delete profile");profileActions->addWidget(create);profileActions->addWidget(erase);layout->addLayout(profileActions);
        connect(presets,&QComboBox::activated,this,[this]{save();QString error;if(!profiles::Store(path).select(config,presets->currentText(),error))notice->setText(error);refresh();});
        connect(create,&QPushButton::clicked,this,[this]{bool ok=false;auto name=QInputDialog::getText(this,"Save profile","Profile name",QLineEdit::Normal,{},&ok);if(!ok)return;save();QString error;if(!profiles::Store(path).create(config,name,error))notice->setText(error);refresh();});
        connect(erase,&QPushButton::clicked,this,[this]{if(QMessageBox::question(this,"Delete profile","Delete the selected profile and restore Global?")!=QMessageBox::Yes)return;save();QString error;if(!profiles::Store(path).remove(config,error))notice->setText(error);refresh();});
        auto toggle=[&](const QString& key,const QString& label){auto box=new QCheckBox(label);toggles[key]=box;layout->addWidget(box);connect(box,&QCheckBox::toggled,this,[this,key](bool enabled){if(refreshing)return;config[key]=enabled;if(enabled&&(key=="hdr_enabled"||key=="sharpen_enabled"))config["enabled"]=true;changed();});};
        toggle("hdr_enabled","Enable &HDR");peak=new QLabel;peak->setWordWrap(true);layout->addWidget(peak);
        auto controls=new QFormLayout;layout->addLayout(controls);
        auto value=[&](const QString& key,const QString& label,int low,int high,const QString& help){auto box=new QSpinBox;box->setRange(low,high);box->setAccessibleDescription(help);box->setToolTip(help);values[key]=box;controls->addRow(label,box);connect(box,&QSpinBox::valueChanged,this,[this,key](int number){if(refreshing)return;config[key]=number;if(key.startsWith("shader_"))config["shader_preset"]="custom";changed();});};
        auto choice=[&](const QString& key,const QString& label,const QList<QPair<QString,QString>>& options,const QString& help){auto box=new QComboBox;for(const auto& option:options)box->addItem(option.second,option.first);box->setAccessibleDescription(help);box->setToolTip(help);choices[key]=box;controls->addRow(label,box);connect(box,&QComboBox::activated,this,[this,key,box]{if(refreshing)return;config[key]=box->currentData().toString();if(key=="shader_curve")config["shader_preset"]="custom";changed();});};
        value("middle_gray","Middle &greys (nits)",10,100,"Brightness of everyday tones without changing the display peak.");
        value("contrast","&Contrast (%)",-100,100,"Separation between bright and dark areas.");value("saturation","&Saturation (%)",-100,100,"Colour intensity. Zero keeps the default balance.");
        choice("hdr_engine","&Engine",{{"auto","Auto - RTX HDR, shader backup"},{"rtx","RTX HDR only"},{"shader","Shader only"}},"Auto uses RTX HDR and switches to the shader engine if RTX HDR cannot start or stops working.");
        engineState=new QLabel;engineState->setWordWrap(true);engineState->setAccessibleName("HDR engine status");layout->addWidget(engineState);
        retryRtx=new QPushButton("Retry RTX HDR");layout->addWidget(retryRtx);connect(retryRtx,&QPushButton::clicked,this,[this]{effect.call("retryRtxEngine");});
        auto tuning=new QLabel("Shader engine tuning");layout->addWidget(tuning);auto shader=new QFormLayout;layout->addLayout(shader);controls=shader;
        auto preset=new QComboBox;for(const auto& item:shaderPresets)preset->addItem(item.label,QString::fromUtf8(item.id));preset->addItem("Custom","custom");preset->setAccessibleDescription("Applies a set of shader engine values.");choices["shader_preset"]=preset;controls->addRow("Pr&eset",preset);
        connect(preset,&QComboBox::activated,this,[this,preset]{if(refreshing)return;for(const auto& item:shaderPresets)if(preset->currentData().toString()==item.id){applyShaderPreset(config,item);changed();save();refresh();return;}});
        choice("shader_curve","&Tone curve",{{"bt2446a","BT.2446 Method A"},{"power","Highlight boost"}},"BT.2446 Method A follows the ITU SDR-to-HDR expansion. Highlight boost keeps SDR tones and lifts only the brightest part of the picture.");
        value("shader_peak","Peak &output (%)",25,100,"Highest brightness the shader engine may use, as a share of the display peak.");value("shader_exposure","E&xposure (0.01 EV)",-200,200,"Scales the SDR picture before tone mapping, in hundredths of a stop.");
        value("shader_saturation","Colour &boost (%)",-100,100,"Extra colour intensity applied together with Saturation.");value("shader_highlight_start","Highlight st&art (%)",30,95,"Highlight boost curve only. SDR signal level where the boost begins.");
        value("shader_highlight_gain","Highlight ga&in (0.01 EV)",0,500,"Highlight boost curve only. Brightness added at SDR white, in hundredths of a stop.");value("shader_shadow_start","Shadow star&t (%)",0,50,"SDR signal level below which Shadow depth applies.");
        value("shader_shadow_crush","Shadow &depth (%)",0,100,"Darkens tones under Shadow start. Zero leaves them unchanged.");
        toggle("debanding","Enable deband&ing");auto bands=new QFormLayout;layout->addLayout(bands);controls=bands;
        value("deband_strength","Filter stren&gth (%)",0,100,"Strength of the adaptive filter. Higher values smooth wider bands.");toggle("deband_before_rtx","Also filter before RTX HDR");
        toggle("sharpen_enabled","Enable s&harpening");auto sharp=new QFormLayout;layout->addLayout(sharp);controls=sharp;
        value("sharpen_intensity","&Intensity (%)",0,100,"Enhances fine edges. Zero turns sharpening off.");value("ignore_film_grain","Ignore &film grain (%)",0,100,"Reduces sharpening of grain and fine texture variations.");toggle("sharpen_adaptive","Edge-a&ware sharpening");
        toggle("reduced_motion","&Reduce overlay motion");
        notice=new QLabel;notice->setWordWrap(true);notice->setAccessibleName("Settings status");layout->addWidget(notice);layout->addStretch();
        auto close=new QPushButton("&Close");outer->addWidget(close);connect(close,&QPushButton::clicked,this,&QWidget::close);
        debounce.setSingleShot(true);connect(&debounce,&QTimer::timeout,this,[this]{save();});
        files.addPath(root);files.addPath(root+"/output/desktop");connect(&files,&QFileSystemWatcher::directoryChanged,this,[this]{refresh();});
        refresh();refreshApplications();
    }
    void closeEvent(QCloseEvent* event)override{save();if(dirty){event->ignore();return;}QWidget::closeEvent(event);}
};
