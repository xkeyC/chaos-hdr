#include "profiles.hpp"
#include "picture_settings.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

int main(int argc, char** argv) {
    QCoreApplication app(argc,argv);
    assert(PictureSettings::supportedPeak(650)==650&&PictureSettings::supportedPeak(1500)==1500);
    assert(PictureSettings::supportedPeak(399)==0&&PictureSettings::supportedPeak(2001)==0);
    assert(PictureSettings::supportedPeak(std::numeric_limits<double>::quiet_NaN())==0);
    assert(ShaderTone::supportedPeak(350)==350&&ShaderTone::supportedPeak(4000)==4000&&ShaderTone::supportedPeak(50)==0);
    assert(hdrEngine({})==HdrEngine::Auto&&hdrEngine({{"hdr_engine","shader"}})==HdrEngine::Shader&&hdrEngine({{"hdr_engine","rtx"}})==HdrEngine::Rtx);
    auto near=[](float a,float b,float limit){return std::abs(a-b)<=limit;};
    auto rising=[](const ShaderTone& tone){float last=-1;for(int n=0;n<=1000;++n){float value=tone.luminance(n/1000.f);if(value+.01f<last)return false;last=value;}return true;};
    for(int peak:{350,650,1020,2000,4000})for(int contrast:{-100,0,100})for(int middle:{10,50,100}){
        auto tone=ShaderTone::from({{"contrast",contrast},{"middle_gray",middle}},peak);
        assert(rising(tone));
        assert(contrast<0?tone.luminance(1)<peak:near(tone.luminance(1),peak,peak*.01f));
        assert(near(tone.luminance(.18f),middle,middle*.08f));
        auto bright=ShaderTone::from({{"contrast",contrast},{"middle_gray",middle},{"shader_curve","power"},{"shader_highlight_gain",500},{"shader_highlight_start",50}},peak);
        assert(rising(bright));assert(bright.luminance(1)<=peak*1.01f);
    }
    auto dim=ShaderTone::from({{"shader_peak",50}},1000);assert(near(dim.luminance(1),500,5));
    auto lifted=ShaderTone::from({{"shader_exposure",100}},1000);assert(lifted.exposure==2.f);
    auto crushed=ShaderTone::from({{"shader_shadow_start",30},{"shader_shadow_crush",100}},1000);assert(crushed.luminance(.01f)<ShaderTone::from({},1000).luminance(.01f));
    auto white=ShaderTone::from({{"shader_saturation",0}},1000).encoded(1,1,1);assert(near(white[0],.7518f,.005f)&&near(white[1],white[0],.001f)&&near(white[2],white[0],.001f));
    auto black=ShaderTone::from({},1000).encoded(0,0,0);assert(black[0]<.001f);
    assert(argc==2);QDir().mkpath(QString::fromLocal8Bit(argv[1]));
    QTemporaryDir folder(QString::fromLocal8Bit(argv[1])+"/profiles-XXXXXX");assert(folder.isValid());
    QString live=folder.path()+"/profile.json", error;
    QJsonObject config{{"enabled",true},{"debanding",true},{"middle_gray",65},{"contrast",0},{"saturation",0},
                       {"sharpen_intensity",50},{"ignore_film_grain",0},{"peak_brightness",1500},{"target_display","DP-3"}};
    profiles::Store store(live);
    assert(store.saveCurrent(config,error));
    assert(!store.create(config,"global",error));
    assert(!store.create(config," \t ",error));
    assert(!store.create(config,QString(65,'x'),error));
    assert(!store.create(config,"bad\nname",error));
    assert(store.create(config,QString::fromUtf8("Night / 夜"),error));
    config["middle_gray"]=70;config["sharpen_intensity"]=20;assert(store.saveCurrent(config,error));
    assert(!store.create(config,QString::fromUtf8("night / 夜"),error));
    assert(store.create(config,"Second",error));
    config["contrast"]=12;

    config["target_display"]="HDMI-A-1";config["peak_brightness"]=1300;
    assert(store.select(config,"Global",error));
    assert(config["middle_gray"].toInt()==65 && config["sharpen_intensity"].toInt()==50 && config["contrast"].toInt()==0);
    assert(config["target_display"].toString()=="HDMI-A-1" && config["peak_brightness"].toInt()==1300);
    assert(!store.remove(config,error));
    assert(store.select(config,"Second",error));assert(config["contrast"].toInt()==12);
    assert(store.select(config,QString::fromUtf8("Night / 夜"),error));assert(config["sharpen_intensity"].toInt()==20);
    assert(store.remove(config,error));assert(profiles::active(config)=="Global");
    assert(config["middle_gray"].toInt()==65 && config["sharpen_intensity"].toInt()==50);
    assert(!store.select(config,"missing",error));
    auto invalid=config;invalid["middle_gray"]=65.5;assert(!store.saveCurrent(invalid,error));
    QFile saved(folder.path()+"/profiles.json");assert(saved.open(QIODevice::ReadOnly));
    auto presets=QJsonDocument::fromJson(saved.readAll()).object()["profiles"].toObject();saved.close();
    for(auto value:presets)assert(!value.toObject().contains("target_display")&&!value.toObject().contains("peak_brightness"));

    assert(config["hdr_enabled"].toBool()&&config["sharpen_enabled"].toBool());
    config["hdr_enabled"]=false;config["sharpen_enabled"]=true;
    assert(store.create(config,"Sharpening only",error));
    assert(store.select(config,"Second",error));
    config["hdr_enabled"]=true;config["sharpen_enabled"]=false;
    assert(store.saveCurrent(config,error));
    assert(store.select(config,"Sharpening only",error));
    assert(!config["hdr_enabled"].toBool()&&config["sharpen_enabled"].toBool());
    assert(store.select(config,"Second",error));
    assert(config["hdr_enabled"].toBool()&&!config["sharpen_enabled"].toBool());
    invalid=config;invalid["hdr_enabled"]="false";assert(!store.saveCurrent(invalid,error));
    invalid=config;invalid["sharpen_enabled"]=0;assert(!store.saveCurrent(invalid,error));
    for(auto bad:{QJsonObject{{"hdr_engine","gpu"}},QJsonObject{{"shader_curve","linear"}},QJsonObject{{"shader_preset","none"}},QJsonObject{{"shader_peak",10}},QJsonObject{{"shader_exposure",12.5}},QJsonObject{{"deband_before_rtx",1}}}){
        invalid=config;for(auto it=bad.begin();it!=bad.end();++it)invalid[it.key()]=it.value();assert(!store.saveCurrent(invalid,error));
    }
    for(const auto& preset:shaderPresets){auto tuned=config;applyShaderPreset(tuned,preset);assert(profiles::valid(profiles::picture(tuned)));}
    assert(profiles::picture(QJsonObject{{"enabled",true}})["hdr_engine"].toString()=="auto");
    auto unset=config;unset.remove("shader_peak");unset.remove("shader_curve");(void)unset["shader_peak"];(void)unset["shader_curve"];
    assert(unset.contains("shader_peak")&&unset["shader_peak"].isNull());
    assert(store.saveCurrent(unset,error));
    auto legacy=profiles::picture(config);legacy.remove("hdr_enabled");legacy.remove("sharpen_enabled");legacy.remove("hdr_engine");legacy.remove("shader_curve");
    assert(profiles::valid(legacy));
    assert(store.persist({{"Global",legacy},{"Second",profiles::picture(config)}},error));
    config["shader_curve"]="power";config["hdr_engine"]="shader";
    assert(store.select(config,"Global",error));
    assert(config["hdr_enabled"].toBool()&&config["sharpen_enabled"].toBool());
    assert(config["hdr_engine"].toString()=="auto"&&config["shader_curve"].toString()=="bt2446a");

    QJsonObject recovered;assert(store.read(config,recovered,error));
    auto pending=config;pending["contrast"]=37;recovered[profiles::active(pending)]=profiles::picture(pending);
    assert(profiles::write(live+".pending",{{"version",1},{"config",pending},{"presets",recovered}},error));
    assert(store.recover(error));assert(!QFile::exists(live+".pending"));
    QFile liveFile(live);assert(liveFile.open(QIODevice::ReadOnly));
    assert(QJsonDocument::fromJson(liveFile.readAll()).object()["contrast"].toInt()==37);liveFile.close();
    QJsonObject verified;assert(store.read(pending,verified,error));assert(verified[profiles::active(pending)].toObject()["contrast"].toInt()==37);
    assert(profiles::write(live+".pending",{{"version",1},{"config",pending},{"presets",QJsonObject{}}},error));
    assert(!store.recover(error));assert(QFile::exists(live+".pending"));assert(!store.saveCurrent(config,error));QFile::remove(live+".pending");
    assert(QFile::remove(live));assert(QDir().mkdir(live));
    pending["contrast"]=41;assert(!store.saveCurrent(pending,error));assert(QFile::exists(live+".pending"));
    assert(QDir().rmdir(live));assert(store.recover(error));
    assert(liveFile.open(QIODevice::ReadOnly));assert(QJsonDocument::fromJson(liveFile.readAll()).object()["contrast"].toInt()==41);liveFile.close();

    assert(saved.open(QIODevice::WriteOnly|QIODevice::Truncate));saved.write("{broken");saved.close();
    assert(!store.create(config,"New",error));assert(!store.saveCurrent(config,error));
    assert(saved.open(QIODevice::ReadOnly));assert(saved.readAll()=="{broken");
    std::cout<<"Profiles: switching, autosave, display independence, deletion, names and corrupt-file preservation passed\n";
}
