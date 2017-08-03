// exfat_mounter.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"

std::vector<uint8_t*> clusters;
exfat_table table;

std::wofstream *logs = nullptr;

std::wstring label_name;

int unknowns = 0;

static off_t s2o(const struct exfat_table* ef, off_t sector)
{
	return sector << ef->sector_bits;
}

/*
* Cluster to sector.
*/
static off_t c2s(const struct exfat_table* ef, int32_t cluster)
{
	if (cluster < EXFAT_FIRST_DATA_CLUSTER)
		std::cerr << "invalid cluster number " << cluster << std::endl;
	return (ef->cluster_sector_start) +
		((off_t)(cluster - EXFAT_FIRST_DATA_CLUSTER) << ef->spc_bits);
}

/*
* Cluster to absolute offset.
*/
off_t exfat_c2o(const struct exfat_table* ef, int32_t cluster)
{
	return s2o(ef, c2s(ef, cluster));
}

/*
* Sector to cluster.
*/
static int32_t s2c(const struct exfat_table* ef, off_t sector)
{
	return ((sector - (ef->cluster_sector_start)) >>
		ef->spc_bits) + EXFAT_FIRST_DATA_CLUSTER;
}

void find_directories(std::ifstream *fin, uint32_t cluster, uint64_t cluster_size, std::wstring folder, int depth = 0) {
	uint8_t * cluster_data = new uint8_t[cluster_size];
	fin->seekg(exfat_c2o(&table, cluster));
	fin->read((char*)cluster_data, cluster_size);

	bool isDirectory = false;
	for (uint64_t i = 0; i < cluster_size; i += 32) {
		switch (((exfat_entry*)&cluster_data[i])->type) {
			case EXFAT_ENTRY_FILE: {
				auto file = ((exfat_entry_meta1*)&cluster_data[i]);
				i += 32;

				auto name_length = ((exfat_entry_meta2*)&cluster_data[i])->name_length;
				auto folder_cluster = ((exfat_entry_meta2*)&cluster_data[i])->start_cluster;
				auto folder_size = ((exfat_entry_meta2*)&cluster_data[i])->size;
				i += 32;

				bool wrong_name = false;

				std::wstring filename;
				if (name_length > 15) {
					std::wstringstream wss;
					wchar_t str[16];
					memcpy_s(str, 30, ((exfat_entry_name*)&cluster_data[i])->name, 30);
					str[15] = 0;
					wss << str;
					for (int n = 0; n < ((name_length - 1) / 15); ++n) {
						i += 32;
						wchar_t str2[16];
						memcpy_s(str2, 30, ((exfat_entry_name*)&cluster_data[i])->name, 30);
						str2[15] = 0;
						wss << str2;
					}
					filename = wss.str();
				}
				else {
					wchar_t str[16];
					memcpy_s(str, 30, ((exfat_entry_name*)&cluster_data[i])->name, 30);
					str[15] = 0;
					filename = str;
				}

				if (file->directory) {
					_wmkdir((std::wstring(L"./volumes/") + label_name + L"/" + folder + L"/" + filename).c_str());
					find_directories(fin, folder_cluster, folder_size, folder + L'/' + filename, depth + 1);
				}
				else {
					std::ofstream fout(std::wstring(L"./volumes/") + label_name + L"/" + folder + L"/" + filename, std::iostream::binary);
					if (folder_cluster != 0 && folder_size > 0) {
						uint8_t * data = new uint8_t[folder_size];
						fin->seekg(exfat_c2o(&table, folder_cluster));
						fin->read((char*)data, folder_size);
						fout.write((char*)data, folder_size);
					}
					fout.close();
				}
				break;
			}
		}
	}
}

int main()
{
	std::ifstream* fin = new std::ifstream("system_fs_image.bin", std::iostream::binary);
	logs = new std::wofstream("logs.txt", std::iostream::binary);

	fin->read((char*)&table, sizeof(exfat_table));
	std::cout << "OEM: " << table.oem_name << std::endl;
	std::cout << "Sector start: 0x" << std::hex << table.sector_start << std::endl;
	std::cout << "Sector count: " << std::dec << table.sector_count << std::endl;

	std::cout << "Fat sector start: 0x" << std::hex << table.fat_sector_start << std::endl;
	std::cout << "Fat sector count: " << std::dec << table.fat_sector_count << std::endl;

	std::cout << "Cluster sector start: 0x" << std::hex << table.cluster_sector_start*(int)SECTOR_SIZE(&table) << std::endl;
	std::cout << "Cluster count: " << std::dec << table.cluster_count << std::endl;

	std::cout << "Rootdir cluster: " << std::dec << table.rootdir_cluster << std::endl;

	std::cout << "Volume serial number: " << std::hex << std::setfill('0') << std::setw(2) << (int)table.volume_serial[0] << (int)table.volume_serial[1] << (int)table.volume_serial[2] << (int)table.volume_serial[3] << std::endl;

	std::cout << "Version: " << std::dec << (int)table.version.vermaj << "." << std::dec << (int)table.version.vermin << std::endl;

	std::cout << "Volume state: " << std::dec << table.volume_state << std::endl << std::endl;
	std::cout << "Bytes per sector: " << std::dec << (int)SECTOR_SIZE(&table) << std::endl;
	std::cout << "Sectors per cluster: " << std::dec << (int)table.spc_bits << std::endl;
	std::cout << "Fat count: " << std::dec << (int)table.fat_count << std::endl << std::endl;
	std::cout << "Drive number: " << std::dec << (int)table.drive_no << std::endl;
	std::cout << "Allocated percents: " << std::dec << (int)((255 / (float)table.allocated_percent)*100) << "%" << std::endl << std::endl;

	std::cout << "Cluster size = " << std::dec << CLUSTER_SIZE(&table)/1024 << " KB" << std::endl << std::endl;

	std::cout << "Allocation cluster sector start: 0x" << std::hex << (table.cluster_sector_start*(int)SECTOR_SIZE(&table)) + CLUSTER_SIZE(&table) << std::endl;

	std::cout << "Allocation cluster offset: " << std::hex << exfat_c2o(&table, 5) << std::endl;
	std::cout << "1st bitmap cluster offset: " << std::hex << exfat_c2o(&table, 2) << std::endl;
	std::cout << "2st bitmap cluster offset: " << std::hex << exfat_c2o(&table, 3) << std::endl;
	std::cout << "Upcase cluster offset: " << std::hex << exfat_c2o(&table, 4) << std::endl;


	volume_label label;
	fin->seekg(0);

	int8_t *rootdir = new int8_t[CLUSTER_SIZE(&table)];
	fin->seekg(exfat_c2o(&table, 5));
	fin->read((char*)rootdir, CLUSTER_SIZE(&table));

	_wmkdir(L".\\volumes\\");

	bool isDirectory = false;
	for (int i = 0; i < CLUSTER_SIZE(&table); i += 32) {
		switch (((exfat_entry*)&rootdir[i])->type) {
			case EXFAT_ENTRY_LABEL:
				std::wcout << L"Volume name: " << ((exfat_entry_label*)&rootdir[i])->name << std::endl;
				label_name = ((exfat_entry_label*)&rootdir[i])->name;
				_wmkdir((std::wstring(L".\\volumes\\") + label_name).c_str());
				break;
			case EXFAT_ENTRY_BITMAP:
				std::wcout << L"Bitmap found: " << ((((exfat_entry_bitmap*)&rootdir[i])->flags == 1) ? "2nd" : "1st") << ", cluster number: " << ((exfat_entry_bitmap*)&rootdir[i])->start_cluster << ", size of bitmap: " << ((exfat_entry_bitmap*)&rootdir[i])->size << std::endl;
				break;
			case EXFAT_ENTRY_UPCASE:
				std::wcout << L"Upcase found: 0x" << std::hex << ((exfat_entry_upcase*)&rootdir[i])->checksum << std::dec << ", cluster number: " << ((exfat_entry_upcase*)&rootdir[i])->start_cluster << ", size of table: " << ((exfat_entry_upcase*)&rootdir[i])->size << std::endl;
				break;
			case EXFAT_ENTRY_FILE: {
				auto file = ((exfat_entry_meta1*)&rootdir[i]);
				i += 32;

				auto folder_cluster = ((exfat_entry_meta2*)&rootdir[i])->start_cluster;
				auto folder_start = ((exfat_entry_meta2*)&rootdir[i])->size;

				i += 32;
				(*logs) << ((exfat_entry_name*)&rootdir[i])->name << std::endl;

				if (file->directory) {
					_wmkdir((std::wstring(L".\\volumes\\") + label_name + L"\\" + ((exfat_entry_name*)&rootdir[i])->name).c_str());
					find_directories(fin, folder_cluster, folder_start, ((exfat_entry_name*)&rootdir[i])->name);
				}
				break;
			}
		}
	}

	getchar();
    return 0;
}

