#ifndef _CALIB_IMPL_H_INCLUDED
#define _CALIB_IMPL_H_INCLUDED

#ifdef CALIB_IMPL
#include <EEPROM.h>


byte* eeprom_read(int start_address, int size) {
	byte *buffer = (byte *)malloc(size);
	for (size_t i=0; i < size; i++) {
		buffer[i] = EEPROM.read(start_address + i);
	} return buffer;
}


void eeprom_write(int start_address, byte* buffer, int size) {
	for (size_t i=0; i < size; i++) {
		EEPROM.write(start_address + i, buffer[i]);
	}
}


bool calibration_check_exists(const calibration_meta_t* data_header, const calibration_meta_t* example_meta) {
	if (strcmp(example_meta->calibration_start,	data_header->calibration_start) != 0) { return false; }
	if (strcmp(example_meta->version,			data_header->version		  ) != 0 && \
	    strcmp(example_meta->version,			_version_skip_key_g			  ) != 0) { return false; }
	if (	   example_meta->sensor_num !=			data_header->sensor_num			 && \
			   example_meta->sensor_num !=			0								) { return false; }
	return true;
}

bool calibration_check_sensor_data(const sensor_data_t* sensor_datas, int sensor_num) {
	for (int i=0; i < sensor_num; i++) {
		if (0 > sensor_datas[i]._min || sensor_datas[i]._min > 1023) { return false; } 
		if (0 > sensor_datas[i]._max || sensor_datas[i]._max > 1023) { return false; } 
		if (0 >= sensor_datas[i]._normal || sensor_datas[i]._normal >= 1023) { return false; } 
		if (sensor_datas[i]._min >= sensor_datas[i]._normal || sensor_datas[i]._normal >= sensor_datas[i]._max) { return false; } 
	} return true;
}

bool calibration_check_equals(const calibration_data_t* data1, const calibration_data_t* data2) {
	if (calibration_check_exists(&data1->header, &data2->header) == false)		{ return false; }
	for (int i=0; i < data1->header.sensor_num; i++) {
		if (data1->sensor_datas[i]._min != data2->sensor_datas[i]._min)			{ return false; }
		if (data1->sensor_datas[i]._max != data2->sensor_datas[i]._max)			{ return false; }
		if (data1->sensor_datas[i]._normal != data2->sensor_datas[i]._normal)	{ return false; }
	} return true;
}

bool calibration_check_eeprom(int eeprom_address, const calibration_meta_t* example_meta) {
	calibration_data_t *read_data = calibration_read(eeprom_address);
	bool rv = calibration_check(read_data, example_meta);
	free(read_data); return rv;
}

bool calibration_check(const calibration_data_t* data, const calibration_meta_t* example_meta) {
	if (!calibration_check_exists(&data->header, example_meta))											{ return false; }
	if (!calibration_check_sensor_data((sensor_data_t *)&data->sensor_datas, example_meta->sensor_num))	{ return false; }
	return true;
}


#ifdef _DEBUG
void calibration_print(const calibration_data_t* data) {
	serialf("Start: %s\nVersion: %s\nSensor Count: %d\n", \
		data->header.calibration_start, data->header.version, data->header.sensor_num);
	for (int i=0; i < data->header.sensor_num; i++) {
		serialfn("\tSensor %d: %d %d %d", data->sensor_datas[i]._min, \
			data->sensor_datas[i]._normal, data->sensor_datas[i]._max);
	}
}
#endif


bool calibrate_sensors(calibration_data_t* _data, int first_sensor, enum encoder_mode_g mode) {
	serialdn("Starting calibration.");
	/* E??er debug modundaysak g??nderilen datay?? kontrol et */
	dbg(if (!calibration_check_exists(&_data->header, &_example_meta_g)) { return false; })
	/* Biraz da olsun h??z kazanmak i??in */
	int sensor_num = _data->header.sensor_num;
	/* Okunan de??erleri kaydetmek i??in kullan??lacak array */
	sensor_data_t *read_values = (sensor_data_t *)malloc(sensor_num * sizeof(sensor_data_t));

	/* ilk okuma (normal de??erleri anlamak i??in) */
	for (int i=0; i < sensor_num; i++) {
		int _pin_val = analogRead(first_sensor+i);
		read_values[i] = { _pin_val, _pin_val, _pin_val };
	}

	/* ufak bir i??aret */
	blink(100, 30);
	/* counter sens??rlerin ka?? kere okundu??unu saymak i??in */
	unsigned long counter = 0, start_time = millis();
	/* toplamda 6 saniye boyunca sens??rlerden veri okunacak  */
	/* ve verilerin olas?? de??er aral??klar?? belirlenecek */
	while (millis() - start_time <= _calibration_length_g * 1000) {
		for (int i=0; i < sensor_num; i++) {
			/* sens??rlerin s??rayla tak??lm???? olmas?? gerekiyor */
			int read_value = analogRead(first_sensor+i);
			/* e??er en k??????k de??erden daha k??????k de??er okunursa en k??????k de??eri de??i??tir */
			if (read_value < read_values[i]._min)		read_values[i]._min = read_value;
			/* yukardakinin ayn??s?? ama en b??y??k i??in */
			else if (read_value > read_values[i]._max)	read_values[i]._max = read_value;
		} counter++;
	/* bitti??ine dair sinyal */
	} blink(100, 30);
	

#ifdef _NOSENSOR
	/* e??er sens??r tak??l?? de??ilse normal de??eri maxla minin ortalamas??na ata */
	for (int i=0; i < sensor_num; i++)
		read_values[i]._normal = (read_values[i]._max - read_values[i]._min) / 2;
#endif

#ifdef _DEBUG
	/* biraz debug info */
	serialfn("Read %d sensors %lu times in %d seconds.", sensor_num, counter, _calibration_length_g);
	for (int i=0; i < sensor_num; i++)
		serialf("Sensor %d:\n\t[min]\t\tread: %d\n\t[normal]\tread: %d\n\t[max]\t\tread: %d\n", i,	\
			read_values[i]._min, read_values[i]._normal, read_values[i]._max);
#endif

	/* e??er okunan veri ge??erli de??ilse de??i??iklikleri verilen dataya yazmadan ????k */
	if (calibration_check_sensor_data(read_values, sensor_num) == 0) { return false; }
	/* okunan veri ge??erliyse dataya yaz */
	memcpy(_data->sensor_datas, read_values, sensor_num * sizeof(sensor_data_t));

	free(read_values);
	return true;
}

#endif
#endif
