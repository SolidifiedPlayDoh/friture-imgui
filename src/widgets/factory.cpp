#include "widgets/widget.hpp"

#include "widgets/delay.hpp"
#include "widgets/generator_widget.hpp"
#include "widgets/longlevels.hpp"
#include "widgets/octave.hpp"
#include "widgets/pitch.hpp"
#include "widgets/scope.hpp"
#include "widgets/spectrogram.hpp"
#include "widgets/spectrum.hpp"

namespace fr {

const char* widgetTypeName(WidgetType t) {
    switch (t) {
        case WidgetType::Scope: return "Scope";
        case WidgetType::FFTSpectrum: return "FFT Spectrum";
        case WidgetType::Spectrogram: return "2D Spectrogram";
        case WidgetType::OctaveSpectrum: return "Octave Spectrum";
        case WidgetType::Generator: return "Generator";
        case WidgetType::DelayEstimator: return "Delay Estimator";
        case WidgetType::LongLevels: return "Long Time Levels";
        case WidgetType::PitchTracker: return "Pitch Tracker";
        default: return "";
    }
}

std::unique_ptr<Widget> createWidget(WidgetType t, Context& ctx) {
    switch (t) {
        case WidgetType::Scope: return std::make_unique<ScopeWidget>(ctx);
        case WidgetType::FFTSpectrum: return std::make_unique<SpectrumWidget>(ctx);
        case WidgetType::Spectrogram: return std::make_unique<SpectrogramWidget>(ctx);
        case WidgetType::OctaveSpectrum: return std::make_unique<OctaveWidget>(ctx);
        case WidgetType::Generator: return std::make_unique<GeneratorWidget>(ctx);
        case WidgetType::DelayEstimator: return std::make_unique<DelayWidget>(ctx);
        case WidgetType::LongLevels: return std::make_unique<LongLevelsWidget>(ctx);
        case WidgetType::PitchTracker: return std::make_unique<PitchWidget>(ctx);
        default: return std::make_unique<SpectrumWidget>(ctx);
    }
}

}  // namespace fr
