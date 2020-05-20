// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "NumberLabel.h"
#include "Lang.h"
#include "libs/StringF.h"
#include "libs/stringUtils.h"

namespace UI {

	NumberLabel::NumberLabel(Context *context, Format format) :
		Label(context, ""),
		m_format(format)
	{
		SetValue(0.0);

		RegisterBindPoint("value", sigc::mem_fun(this, &NumberLabel::BindValue));
		RegisterBindPoint("valuePercent", sigc::mem_fun(this, &NumberLabel::BindValuePercent));
	}

	NumberLabel *NumberLabel::SetValue(double v)
	{
		m_value = v;
		switch (m_format) {
		case FORMAT_NUMBER_2DP:
			SetText(to_string(v, "f.2"));
			break;

		case FORMAT_INTEGER:
			SetText(to_string(uint32_t(v + 0.5), "u"));
			break;

		case FORMAT_PERCENT:
			SetText(stringf("%0{f.2}%%", v * 100.0));
			break;

		case FORMAT_PERCENT_INTEGER:
			SetText(stringf("%0{u}%%", uint32_t(v * 100.0 + 0.5)));
			break;

		case FORMAT_MASS_TONNES:
			SetText(stringf(Lang::NUMBER_TONNES, formatarg("mass", v)));
			break;

		case FORMAT_MONEY:
			SetText(stringUtils::format_money(int64_t(v * 100)));
			break;

		case FORMAT_DISTANCE_M:
			SetText(stringUtils::format_distance(v, 3));
			break;

		case FORMAT_DISTANCE_LY:
			SetText(stringf(Lang::NUMBER_LY, formatarg("distance", v)));
			break;

		default:
		case FORMAT_NUMBER:
			SetText(to_string(v, "f"));
			break;
		}

		return this;
	}

	void NumberLabel::BindValue(PropertyMap &p, const std::string &k)
	{
		double v = 0.0;
		p.Get(k, v);
		SetValue(v);
	}

	void NumberLabel::BindValuePercent(PropertyMap &p, const std::string &k)
	{
		double v = 0.0;
		p.Get(k, v);
		SetValue(Clamp(v, 0.0, 100.0) * 0.01);
	}

} // namespace UI
