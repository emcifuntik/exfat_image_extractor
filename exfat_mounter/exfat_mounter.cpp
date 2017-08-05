// exfat_mounter.cpp: определяет точку входа для консольного приложения.
//

#include "stdafx.h"

std::vector<uint8_t*> clusters;
exfat_table table;
fat32_table ftable;

uint32_t total_sectors = 0,
	fat_size = 0,
	root_dir_sectors = 0,
	first_data_sector = 0,
	first_fat_sector = 0,
	data_sectors = 0,
	total_clusters = 0,
	root_cluster_32 = 0;

std::wstring label_name;

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

#ifdef _WIN32

static FILETIME FatTimeToFileTime(uint16_t date, uint16_t time) {
	FILETIME ft;
	SYSTEMTIME st;

	int day = 0,
		month = 0,
		year = 0,
		second = 0,
		minute = 0,
		hour = 0;

	int offset = 0;
	for(int i = 0; i < 5; ++i, ++offset)
		if((date >> offset) & 1)
			day |= 1 << i;

	for (int i = 0; i < 4; ++i, ++offset)
		if ((date >> offset) & 1)
			month |= 1 << i;

	for (int i = 0; i < 7; ++i, ++offset)
		if ((date >> offset) & 1)
			year |= 1 << i;
	year += 1980;

	offset = 0;
	for (int i = 0; i < 5; ++i, ++offset)
		if ((time >> offset) & 1)
			second |= 1 << i;
	second *= 2;

	for (int i = 0; i < 6; ++i, ++offset)
		if ((time >> offset) & 1)
			minute |= 1 << i;

	for (int i = 0; i < 5; ++i, ++offset)
		if ((time >> offset) & 1)
			hour |= 1 << i;

	st.wYear = year;
	st.wMonth = month;
	st.wDay = day;
	st.wHour = hour;
	st.wMinute = minute;
	st.wSecond = second;

	SystemTimeToFileTime(&st, &ft);
	return ft;
}

#endif

void find_directories(std::ifstream *fin, uint32_t cluster, uint64_t cluster_size, std::wstring folder, int depth = 0) {
	uint8_t * cluster_data = new uint8_t[cluster_size];
	fin->seekg(exfat_c2o(&table, cluster));
	fin->read((char*)cluster_data, cluster_size);

	bool isDirectory = false;
	for (uint64_t i = 0; i < cluster_size; i += 32) {
		switch (((exfat_entry*)&cluster_data[i])->type) {
			case EXFAT_ENTRY_FILE: {
				auto file = ((exfat_entry_meta1*)&cluster_data[i]);
				auto creation_date = ((exfat_entry_meta1*)&cluster_data[i])->crdate;
				auto creation_time = ((exfat_entry_meta1*)&cluster_data[i])->crtime;
				auto mod_date = ((exfat_entry_meta1*)&cluster_data[i])->mdate;
				auto mod_time = ((exfat_entry_meta1*)&cluster_data[i])->mtime;
				auto access_date = ((exfat_entry_meta1*)&cluster_data[i])->adate;
				auto access_time = ((exfat_entry_meta1*)&cluster_data[i])->atime;

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
#ifdef _WIN32
					FILETIME cft = FatTimeToFileTime(creation_date, creation_time);
					FILETIME mft = FatTimeToFileTime(mod_date, mod_time);
					FILETIME aft = FatTimeToFileTime(access_date, access_time);
					HANDLE hFile = CreateFile((std::wstring(L"./volumes/") + label_name + L"/" + folder + L"/" + filename).c_str(),
						FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL, NULL);
					SetFileTime(hFile, &cft, &aft, &mft);
					CloseHandle(hFile);
#endif
				}
				break;
			}
		}
	}
}

static inline void ltrim(std::wstring &s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
		return ch != L'\x16' && ch != L'\x20';
	}));
}

static inline void rtrim(std::wstring &s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
		return ch != L'\x16' && ch != L'\x20';
	}).base(), s.end());
}

static inline void trim(std::wstring &s) {
	ltrim(s);
	rtrim(s);
}

void extractFiles(std::ifstream* fin, uint32_t sector_number, uint32_t sector_size, std::wstring path = L"") {
	uint32_t sector_offset = (((sector_number - 2) * ftable.sector_per_cluster) + first_data_sector) * ftable.sector_bytes;
	fin->seekg(sector_offset);
	uint8_t *sector = new uint8_t[ftable.sector_per_cluster * ftable.sector_bytes];
	fin->read((char*)sector, sector_size);

	for (uint32_t i = 0; i < sector_size; i += 32) {
		if (sector[i] != 0) {
			wchar_t entry_name[MAX_PATH] = { 0 };
			std::wstring name = L"";
			if (sector[i] == 0x2E && (sector[i + 1] == 0x2E || sector[i + 1] == 0x20) && sector[i + 2] == 0x20 && sector[i + 3] == 0x20 && sector[i + 4] == 0x20 && sector[i + 5] == 0x20 && sector[i + 6] == 0x20)
				continue;
			int16_t offset = 0;
			wchar_t name_tail[13] = { 0 };

			fat32_long_directory *ld = (fat32_long_directory*)&sector[i];

			if (sector[i] & 0x40 && ld->flags == FAT32_FLAG_LFN) {
				uint32_t fronts = sector[i] ^ 0x40;
				memcpy_s(&name_tail[0], 10, ld->file_name, 10);
				memcpy_s(&name_tail[5], 12, ld->file_name2, 12);
				memcpy_s(&name_tail[11], 4, ld->file_name3, 4);
				fronts--;
				i += 32;
				for (uint32_t front = 0; front < fronts; ++front, i += 32) {
					ld = (fat32_long_directory*)&sector[i];
					memcpy_s(&entry_name[offset], 10, ld->file_name, 10);offset += 5;
					memcpy_s(&entry_name[offset], 12, ld->file_name2, 12);offset += 6;
					memcpy_s(&entry_name[offset], 4, ld->file_name3, 4);offset += 2;
				}
				name += entry_name;
				name += name_tail;
			}
			else
				offset = -1;

			fat32_file *file = (fat32_file*)&sector[i];
			if (offset == -1) {
				std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
				name = converter.from_bytes(file->file_name);
				trim(name);
			}		

			uint32_t next_cluster = (file->hi_first_cluster << 8) + file->lo_first_cluster;
			if (file->flags & FAT32_FLAG_DIRECTORY) {
				_wmkdir((path + name).c_str());
				extractFiles(fin, next_cluster, ftable.sector_per_cluster * ftable.sector_bytes, path + name + L"\\");
			}
			else {
				uint32_t cluster_offset = (((next_cluster - 2) * ftable.sector_per_cluster) + first_data_sector) * ftable.sector_bytes;
				fin->seekg(cluster_offset);
				uint8_t *file_data = new uint8_t[file->file_size];
				fin->read((char*)file_data, file->file_size);
				std::ofstream fout(path + name, std::ofstream::binary);
				fout.write((char*)file_data, file->file_size);
				fout.close();
			}
		}
	}
};

int main(int argc, char* argv[])
{
	std::ifstream* fin = nullptr;
	if (argc < 2) {
		std::cout << "[USAGE] exfat_mounter.exe [Input file]" << std::endl;
		return 0;
	}
	else {
		fin = new std::ifstream(argv[1], std::iostream::binary);
	}
	uint8_t data[3];
	fin->read((char*)data, 3);

	if (data[0] == 0xEB && data[1] == 0xFE && data[2] == 0x90) {
		fin->seekg(0);
		fin->read((char*)&ftable, sizeof(fat32_table));
#ifdef _DEBUG
		std::cout << "OEM: " << ftable.oem_name << std::endl;
		std::cout << "Volume label: " << ftable.volume_label << std::endl;
		/*std::cout << "Sector start: 0x" << std::hex << ftable.sector_start << std::endl;
		std::cout << "Sector count: " << std::dec << ftable.sector_count << std::endl;
		std::cout << "Fat sector start: 0x" << std::hex << ftable.fat_sector_start << std::endl;
		std::cout << "Fat sector count: " << std::dec << ftable.fat_sector_count << std::endl;
		std::cout << "Cluster sector start: 0x" << std::hex << ftable.cluster_sector_start*(int)SECTOR_SIZE(&table) << std::endl;
		std::cout << "Cluster count: " << std::dec << ftable.cluster_count << std::endl;
		std::cout << "Rootdir cluster: " << std::dec << ftable.rootdir_cluster << std::endl;
		std::cout << "Volume state: " << std::dec << ftable.volume_state << std::endl << std::endl;
		std::cout << "Bytes per sector: " << std::dec << (int)SECTOR_SIZE(&ftable) << std::endl;
		std::cout << "Sectors per cluster: " << std::dec << (int)ftable.spc_bits << std::endl;
		std::cout << "Fat count: " << std::dec << (int)ftable.fat_count << std::endl << std::endl;
		std::cout << "Drive number: " << std::dec << (int)ftable.drive_no << std::endl;
		std::cout << "Allocated percents: " << std::dec << (int)((255 / (float)ftable.allocated_percent) * 100) << "%" << std::endl << std::endl;

		std::cout << "Cluster size = " << std::dec << CLUSTER_SIZE(&table) / 1024 << " KB" << std::endl << std::endl;*/
#endif
		total_sectors = ftable.sectors_on_volume;
		fat_size = ftable.fat_size_in_sectors;
		root_dir_sectors = ((ftable.directory_entries * 32) + (ftable.sector_bytes - 1)) / ftable.sector_bytes;
		first_data_sector = ftable.reserved_sectors + (ftable.fat_count * fat_size) + root_dir_sectors;
		first_fat_sector = ftable.reserved_sectors;
		data_sectors = ftable.total_sectors - (ftable.reserved_sectors + (ftable.fat_count * fat_size) + root_dir_sectors);
		total_clusters = data_sectors / ftable.sector_per_cluster;
		root_cluster_32 = ftable.root_cluster;

		_wmkdir(L".\\volumes");
		_mkdir((std::string(".\\volumes\\") + ftable.volume_label).c_str());
		_chdir((std::string(".\\volumes\\") + ftable.volume_label).c_str());

		extractFiles(fin, root_cluster_32, ftable.sector_per_cluster * ftable.sector_bytes);
		
	}
	else if (data[0] == 0xEB && data[1] == 0x76 && data[2] == 0x90) {
		fin->seekg(0);
		fin->read((char*)&table, sizeof(exfat_table));
#ifdef _DEBUG
		std::cout << "OEM: " << table.oem_name << std::endl;
		std::cout << "Sector start: 0x" << std::hex << table.sector_start << std::endl;
		std::cout << "Sector count: " << std::dec << table.sector_count << std::endl;
		std::cout << "Fat sector start: 0x" << std::hex << table.fat_sector_start << std::endl;
		std::cout << "Fat sector count: " << std::dec << table.fat_sector_count << std::endl;
		std::cout << "Cluster sector start: 0x" << std::hex << table.cluster_sector_start*(int)SECTOR_SIZE(&table) << std::endl;
		std::cout << "Cluster count: " << std::dec << table.cluster_count << std::endl;
		std::cout << "Rootdir cluster: " << std::dec << table.rootdir_cluster << std::endl;
		std::cout << "Volume serial number: " << std::hex << std::setfill('0') << std::setw(2) << (int)table.volume_serial[0] << std::setfill('0') << std::setw(2) << (int)table.volume_serial[1] << std::setfill('0') << std::setw(2) << (int)table.volume_serial[2] << std::setfill('0') << std::setw(2) << (int)table.volume_serial[3] << std::endl;
		std::cout << "Version: " << std::dec << (int)table.version.vermaj << "." << std::dec << (int)table.version.vermin << std::endl;
		std::cout << "Volume state: " << std::dec << table.volume_state << std::endl << std::endl;
		std::cout << "Bytes per sector: " << std::dec << (int)SECTOR_SIZE(&table) << std::endl;
		std::cout << "Sectors per cluster: " << std::dec << (int)table.spc_bits << std::endl;
		std::cout << "Fat count: " << std::dec << (int)table.fat_count << std::endl << std::endl;
		std::cout << "Drive number: " << std::dec << (int)table.drive_no << std::endl;
		std::cout << "Allocated percents: " << std::dec << (int)((255 / (float)table.allocated_percent) * 100) << "%" << std::endl << std::endl;

		std::cout << "Cluster size = " << std::dec << CLUSTER_SIZE(&table) / 1024 << " KB" << std::endl << std::endl;
#endif

		fin->seekg(0);
		int8_t *rootdir = new int8_t[CLUSTER_SIZE(&table)];
		fin->seekg(exfat_c2o(&table, 5));
		fin->read((char*)rootdir, CLUSTER_SIZE(&table));

		_wmkdir(L".\\volumes\\");

		bool isDirectory = false;
		for (int i = 0; i < CLUSTER_SIZE(&table); i += 32) {
			switch (((exfat_entry*)&rootdir[i])->type) {
			case EXFAT_ENTRY_LABEL:
				label_name = ((exfat_entry_label*)&rootdir[i])->name;
				_wmkdir((std::wstring(L".\\volumes\\") + label_name).c_str());
				std::wcout << L"Volume found: " << label_name << std::endl;
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
				if (file->directory) {
					_wmkdir((std::wstring(L".\\volumes\\") + label_name + L"\\" + ((exfat_entry_name*)&rootdir[i])->name).c_str());
					find_directories(fin, folder_cluster, folder_start, ((exfat_entry_name*)&rootdir[i])->name);
				}
				break;
			}
			}
		}
		std::wcout << L"Extracted" << std::endl;
	}
	else {
		std::cout << "0x" << std::hex << (int)data[0] << " 0x" << std::hex << (int)data[1] << " 0x" << std::hex << (int)data[2] << std::endl;
	}
    return 0;
}

