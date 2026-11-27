#pragma once
// Octave / fractional-octave band analyzer (port of friture/octavespectrum.py).

#include <vector>

#include "dsp/filters.hpp"
#include "widgets/widget.hpp"

namespace fr {

class OctaveWidget : public Widget {
public:
    explicit OctaveWidget(Context& ctx);
    const char* name() const override { return "Octave Spectrum"; }
    void onData(const Chunk& chunk) override;
    void draw(ImVec2 size) override;
    void drawSettings() override;
    void save(Settings& s, const std::string& p) const override;
    void load(const Settings& s, const std::string& p) override;

private:
    void updateResponse();
    void setBands(int index);

    OctaveFilterBank bank_;
    int bandsIndex_ = 1;  // 3 bands per octave
    int specMin_ = -80, specMax_ = -20;
    int weighting_ = 0;
    int responseIndex_ = 3;  // 1 s

    std::vector<double> alphas_, sp_, db_;
    std::vector<std::vector<double>> y_;
};

}  // namespace fr
