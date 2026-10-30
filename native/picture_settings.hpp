#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <QJsonObject>
#include <QString>

enum class HdrEngine {Auto,Rtx,Shader};
inline HdrEngine hdrEngine(const QJsonObject& config){
    auto value=config["hdr_engine"].toString("auto");
    return value=="rtx"?HdrEngine::Rtx:value=="shader"?HdrEngine::Shader:HdrEngine::Auto;
}

struct PictureSettings {
    int peak=1500,middle=50,contrast=0,saturation=0;
    bool debanding=true;
    static int supportedPeak(double peak){return std::isfinite(peak)&&peak>=400&&peak<=2000?int(std::round(peak)):0;}
    bool operator==(const PictureSettings&)const=default;
    static PictureSettings from(const QJsonObject& config,int peak){
        return {std::clamp(peak,400,2000),std::clamp(config["middle_gray"].toInt(50),10,100),
                std::clamp(config["contrast"].toInt(),-100,100),std::clamp(config["saturation"].toInt(),-100,100),
                config["debanding"].toBool(true)};
    }
};

struct ShaderPreset {const char* id;const char* label;const char* curve;int exposure,peak,saturation,highlightStart,highlightGain,shadowStart,shadowCrush;};
inline constexpr std::array<ShaderPreset,6> shaderPresets{{
    {"standard","Standard","bt2446a",0,100,10,75,100,15,0},
    {"cinema","Cinema","bt2446a",0,100,0,75,100,15,10},
    {"high_dynamic","High dynamic","bt2446a",0,100,25,75,100,20,20},
    {"soft","Soft natural","power",0,75,5,70,100,15,0},
    {"bright","Bright highlights","power",0,100,15,75,150,15,0},
    {"vivid","Vivid","power",0,100,20,55,200,15,15},
}};
inline void applyShaderPreset(QJsonObject& config,const ShaderPreset& preset){
    config["shader_preset"]=preset.id;config["shader_curve"]=preset.curve;config["shader_exposure"]=preset.exposure;
    config["shader_peak"]=preset.peak;config["shader_saturation"]=preset.saturation;
    config["shader_highlight_start"]=preset.highlightStart;config["shader_highlight_gain"]=preset.highlightGain;
    config["shader_shadow_start"]=preset.shadowStart;config["shader_shadow_crush"]=preset.shadowCrush;
}

struct ShaderTone {
    static constexpr float pSdr=5.696958f;
    bool power=false,debanding=true,debandBeforeRtx=false,adaptiveSharpen=false;
    float deband=.0104f,exposure=1,peak=1000,middle=50,midGamma=1,contrast=1,saturation=1,pHdr=13.26f;
    float highlightStart=.75f,highlightGain=2,shadowStart=0,shadowPower=1,knee=600,slope=1,top=1000;
    static int supportedPeak(double peak){return std::isfinite(peak)&&peak>=100&&peak<=10000?int(std::round(peak)):0;}
    float signal(float linear)const{
        float s=std::pow(std::clamp(linear,0.f,1.f),1/2.4f);
        return s<shadowStart?shadowStart*std::pow(s/shadowStart,shadowPower):s;
    }
    float expanded(float s)const{
        if(power){
            float x=std::clamp((s-highlightStart)/std::max(1-highlightStart,.001f),0.f,1.f);x=x*x*(3-2*x);
            return middle/.18f*std::pow(s,2.4f)*(1+(highlightGain-1)*x);
        }
        float c=std::log(1+(pSdr-1)*s)/std::log(pSdr);
        float p=c<=.7735f?c/1.077f:std::min((2.7811f-std::sqrt(std::max(2.7811f*2.7811f-4*1.151f*(.6302f+c),0.f)))/(2*1.151f),1.f);
        float h=(std::exp(p*std::log(pHdr))-1)/(pHdr-1);
        return peak*std::pow(std::pow(h,2.4f),midGamma);
    }
    float shaped(float nits)const{
        nits=middle*std::pow(nits/middle,contrast);
        if(slope>1&&nits>knee){
            float x=std::min((nits-knee)/(top-knee),1.f);
            nits=knee+(peak-knee)*slope*x/(1+(slope-1)*x);
        }
        return nits;
    }
    float luminance(float linear)const{return shaped(expanded(signal(linear)));}
    static ShaderTone from(const QJsonObject& config,int displayPeak){
        ShaderTone tone;
        tone.power=config["shader_curve"].toString("bt2446a")=="power";
        tone.debanding=config["debanding"].toBool(true);
        tone.debandBeforeRtx=tone.debanding&&config["deband_before_rtx"].toBool(false);
        tone.adaptiveSharpen=config["sharpen_adaptive"].toBool(false);
        tone.deband=.002f+.028f*std::clamp(config["deband_strength"].toInt(30),0,100)/100.f;
        tone.exposure=std::pow(2.f,std::clamp(config["shader_exposure"].toInt(0),-200,200)/100.f);
        tone.middle=std::clamp(config["middle_gray"].toInt(50),10,100);
        tone.peak=std::max(float(displayPeak)*std::clamp(config["shader_peak"].toInt(100),25,100)/100.f,tone.middle*1.5f);
        tone.contrast=1+std::clamp(config["contrast"].toInt(),-100,100)/200.f;
        tone.saturation=(1+std::clamp(config["saturation"].toInt(),-100,100)/200.f)*(1+std::clamp(config["shader_saturation"].toInt(10),-100,100)/100.f);
        tone.pHdr=1+32*std::pow(tone.peak/10000,1/2.4f);
        tone.highlightStart=std::clamp(config["shader_highlight_start"].toInt(75),30,95)/100.f;
        tone.highlightGain=std::pow(2.f,std::clamp(config["shader_highlight_gain"].toInt(100),0,500)/100.f);
        float middleGray=tone.expanded(tone.signal(.18f));
        if(!tone.power){
            float exponent=std::log(tone.middle/tone.peak)/std::log(std::min(middleGray,tone.peak*.999f)/tone.peak);
            tone.midGamma=std::clamp(exponent,.25f,4.f);
        }
        tone.shadowStart=std::clamp(config["shader_shadow_start"].toInt(15),0,50)/100.f;
        tone.shadowPower=1+2*std::clamp(config["shader_shadow_crush"].toInt(0),0,100)/100.f;
        float white=tone.power?tone.middle/.18f*tone.highlightGain:tone.peak;
        tone.top=tone.middle*std::pow(white/tone.middle,tone.contrast);
        tone.knee=tone.peak*.6f;
        tone.slope=tone.top>tone.peak?(tone.top-tone.knee)/(tone.peak-tone.knee):1;
        return tone;
    }
    std::array<float,3> encoded(float r,float g,float b)const{
        auto linear=[](float c){return c<=.04045f?c/12.92f:std::pow((c+.055f)/1.055f,2.4f);};
        float red=linear(r)*exposure,green=linear(g)*exposure,blue=linear(b)*exposure;
        float y=.2126f*red+.7152f*green+.0722f*blue,gain=y>1e-5f?luminance(y)/y:0;
        red*=gain;green*=gain;blue*=gain;
        if(std::abs(saturation-1)>.001f){
            auto root=[](float c){return std::cbrt(std::max(c/10000,1e-9f));};
            float lc=root(.4122214708f*red+.5363325363f*green+.0514459929f*blue);
            float mc=root(.2119034982f*red+.6806995451f*green+.1073969566f*blue);
            float sc=root(.0883024619f*red+.2817188376f*green+.6299787005f*blue);
            float light=.2104542553f*lc+.793617785f*mc-.0040720468f*sc;
            float a=(1.9779984951f*lc-2.428592205f*mc+.4505937099f*sc)*saturation;
            float o=(.0259040371f*lc+.7827717662f*mc-.808675766f*sc)*saturation;
            lc=light+.3963377774f*a+.2158037573f*o;mc=light-.1055613458f*a-.0638541728f*o;sc=light-.0894841775f*a-1.291485548f*o;
            lc=lc*lc*lc*10000;mc=mc*mc*mc*10000;sc=sc*sc*sc*10000;
            red=4.0767416621f*lc-3.3077115913f*mc+.2309699292f*sc;
            green=-1.2684380046f*lc+2.6097574011f*mc-.3413193965f*sc;
            blue=-.0041960863f*lc-.7034186147f*mc+1.707614701f*sc;
        }
        auto pq=[](float nits){
            float v=std::pow(std::clamp(nits/10000,0.f,1.f),.1593017578125f);
            return std::pow((.8359375f+18.8515625f*v)/(1+18.6875f*v),78.84375f);
        };
        return {pq(.6274f*red+.3293f*green+.0433f*blue),pq(.0691f*red+.9195f*green+.0114f*blue),pq(.0164f*red+.088f*green+.8956f*blue)};
    }
};
