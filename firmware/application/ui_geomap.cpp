/*
 * Copyright (C) 2015 Jared Boone, ShareBrained Technology, Inc.
 * Copyright (C) 2017 Furrtek
 * 
 * This file is part of PortaPack.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include "ui_geomap.hpp"

#include "portapack.hpp"

#include <cstring>
#include <stdio.h>

using namespace portapack;

namespace ui {

GeoPos::GeoPos(
	const Point pos
) {
	set_parent_rect({pos, {30 * 8, 3 * 16}});
	
	add_children({
		&labels_position,
		&field_altitude,
		&field_lat_degrees,
		&field_lat_minutes,
		&field_lat_seconds,
		&field_lon_degrees,
		&field_lon_minutes,
		&field_lon_seconds
	});
	
	// Defaults
	set_altitude(0);
	set_lat(0);
	set_lon(0);
	
	const auto changed_fn = [this](int32_t) {
		if (on_change && report_change)
			on_change(altitude(), lat(), lon());
	};
	
	field_altitude.on_change = changed_fn;
	field_lat_degrees.on_change = changed_fn;
	field_lat_minutes.on_change = changed_fn;
	field_lat_seconds.on_change = changed_fn;
	field_lon_degrees.on_change = changed_fn;
	field_lon_minutes.on_change = changed_fn;
	field_lon_seconds.on_change = changed_fn;
}

// Stupid hack to avoid an event loop
void GeoPos::set_report_change(bool v) {
	report_change = v;
}

void GeoPos::focus() {
	field_altitude.focus();
}

void GeoPos::set_altitude(int32_t altitude) {
	field_altitude.set_value(altitude);
}

void GeoPos::set_lat(float lat) {
	field_lat_degrees.set_value(lat);
	field_lat_minutes.set_value((uint32_t)abs(lat / (1.0 / 60)) % 60);
	field_lat_seconds.set_value((uint32_t)abs(lat / (1.0 / 3600)) % 60);
}

void GeoPos::set_lon(float lon) {
	field_lon_degrees.set_value(lon);
	field_lon_minutes.set_value((uint32_t)abs(lon / (1.0 / 60)) % 60);
	field_lon_seconds.set_value((uint32_t)abs(lon / (1.0 / 3600)) % 60);
}

float GeoPos::lat() {
	return field_lat_degrees.value() + (field_lat_minutes.value() / 60.0) + (field_lat_seconds.value() / 3600.0);
};

float GeoPos::lon() {
	return field_lon_degrees.value() + (field_lon_minutes.value() / 60.0) + (field_lon_seconds.value() / 3600.0);
};

int32_t GeoPos::altitude() {
	return field_altitude.value();
};

void GeoPos::set_read_only(bool v) {
	set_focusable(~v);
};

GeoMap::GeoMap(
	Rect parent_rect
) : Widget { parent_rect }
{
	//set_focusable(true);
}

void GeoMap::paint(Painter& painter) {
	Coord line;
	std::array<ui::Color, 240> map_line_buffer;
	//Color border;
	const auto r = screen_rect();
	
	// Ony redraw map if it moved by at least 1 pixel
	if ((x_pos != prev_x_pos) || (y_pos != prev_y_pos)) {
		for (line = 0; line < r.height(); line++) {
			map_file.seek(4 + ((x_pos + (map_width * (y_pos + line))) << 1));
			map_file.read(map_line_buffer.data(), r.width() << 1);
			display.draw_pixels({ 0, r.top() + line, r.width(), 1 }, map_line_buffer);
		}
		
		prev_x_pos = x_pos;
		prev_y_pos = y_pos;
	}
	
	if (mode_ == PROMPT) {
		// Cross
		display.fill_rectangle({ r.center() - Point(16, 1), { 32, 2 } }, Color::red());
		display.fill_rectangle({ r.center() - Point(1, 16), { 2, 32 } }, Color::red());
	} else {
		draw_bearing({ 120, 32 + 144 }, angle_, 16, Color::red());
	}
	
	/*if (has_focus() || highlighted())
		border = style().foreground;
	else
		border = Color::grey();
	
	painter.draw_rectangle(
		{ r.location().x(), r.location().y(), r.size().width(), r.size().height() },
		border
	);*/
}

bool GeoMap::on_touch(const TouchEvent event) {
	if (event.type == TouchEvent::Type::Start) {
		set_highlighted(true);
		if (on_move) {
			Point p = event.point - screen_rect().center();
			on_move(p.x() / 2.0 * lon_ratio, p.y() / 2.0 * lat_ratio);
			return true;
		}
	}
	return false;
}

void GeoMap::move(const float lon, const float lat) {
	lon_ = lon;
	lat_ = lat;
	
	Rect map_rect = screen_rect();
	
	// Map is in Equidistant "Plate Carrée" projection
	x_pos = map_center_x - (map_rect.width() / 2) + (lon_ / lon_ratio);
	y_pos = map_center_y - (map_rect.height() / 2) + (lat_ / lat_ratio);
	
	// Cap position
	if (x_pos > (map_width - map_rect.width()))
		x_pos = map_width - map_rect.width();
	if (y_pos > (map_height + map_rect.height()))
		y_pos = map_height - map_rect.height();
}

bool GeoMap::init() {
	auto result = map_file.open("ADSB/world_map.bin");
	if (result.is_valid()) {
		return false;
	}
	
	map_file.read(&map_width, 2);
	map_file.read(&map_height, 2);
	
	map_center_x = map_width >> 1;
	map_center_y = map_height >> 1;
	
	lon_ratio = 180.0 / map_center_x;
	lat_ratio = 90.0 / map_center_y;
	
	return true;
}

void GeoMap::set_mode(GeoMapMode mode) {
	mode_ = mode;
}

void GeoMap::draw_bearing(const Point origin, const uint32_t angle, uint32_t size, const Color color) {
	Point arrow_a, arrow_b, arrow_c;
	
	for (size_t thickness = 0; thickness < 3; thickness++) {
		arrow_a = polar_to_point(angle, size) + origin;
		arrow_b = polar_to_point(angle + 180 - 30, size) + origin;
		arrow_c = polar_to_point(angle + 180 + 30, size) + origin;
		
		display.draw_line(arrow_a, arrow_b, color);
		display.draw_line(arrow_b, arrow_c, color);
		display.draw_line(arrow_c, arrow_a, color);
		
		size--;
	}
}

void GeoMapView::focus() {
	if (!file_error) {
		geopos.focus();
	} else
		nav_.display_modal("No map", "No world_map.bin file in\n/ADSB/ directory", ABORT, nullptr);
}

void GeoMapView::setup() {
	add_children({
		&geopos,
		&geomap
	});
	
	geopos.set_altitude(altitude_);
	geopos.set_lat(lat_);
	geopos.set_lon(lon_);
	
	geopos.on_change = [this](int32_t altitude, float lat, float lon) {
		altitude_ = altitude;
		lat_ = lat;
		lon_ = lon;
		geomap.move(lon_, lat_);
		geomap.set_dirty();
	};
	
	geomap.on_move = [this](float move_x, float move_y) {
		lon_ += move_x;
		lat_ += move_y;
		
		// Stupid hack to avoid an event loop
		geopos.set_report_change(false);
		geopos.set_lon(lon_);
		geopos.set_lat(lat_);
		geopos.set_report_change(true);
		
		geomap.move(lon_, lat_);
		geomap.set_dirty();
	};
}

// Display mode
GeoMapView::GeoMapView(
	NavigationView& nav,
	std::string* tag,
	int32_t altitude,
	float lat,
	float lon,
	float angle
) : nav_ (nav),
	tag_ (tag),
	altitude_ (altitude),
	lat_ (lat),
	lon_ (lon),
	angle_ (angle)
{
	mode_ = DISPLAY;
	
	file_error = !geomap.init();
	if (file_error) return;
	
	setup();
	
	geomap.set_mode(mode_);
	geomap.move(lon_, lat_);
	
	geopos.set_read_only(true);
}

// Prompt mode
GeoMapView::GeoMapView(
	NavigationView& nav,
	int32_t altitude,
	float lat,
	float lon,
	const std::function<void(int32_t, float, float)> on_done
) : nav_ (nav),
	altitude_ (altitude),
	lat_ (lat),
	lon_ (lon)
{
	mode_ = PROMPT;
	
	file_error = !geomap.init();
	if (file_error) return;
	
	setup();
	add_child(&button_ok);
	
	geomap.set_mode(mode_);
	geomap.move(lon_, lat_);
	
	button_ok.on_select = [this, on_done, &nav](Button&) {
		if (on_done)
			on_done(altitude_, lat_, lon_);
		nav.pop();
	};
}

} /* namespace ui */
