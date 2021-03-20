#include "decode_secret.h"
#include "log.h"

#include <array>
#include <string>
#include <vector>
#include <cstring>
#include <cmath>
#include <assert.h>

secret_result_t decode_secret(const void* data, size_t size)
{
	const int8_t* secret = reinterpret_cast<const int8_t*>(data);
	(void)size;  // size_t secret_size = size / sizeof(*secret);
	secret_result_t result{};
	int32_t globalValue = 0;
	for (size_t idx = 0; idx < 24; idx++)
	{
		int32_t value1 = secret[17 + idx * 2];
		if (value1 <= 96) {
			value1 += 32;
		}
		value1 = (value1 - 98 - (idx * 34));
		value1 = value1 % 26;
		if (value1 < 0) {
			value1 += 26;
		}

		int32_t value2 = secret[18 + idx * 2];
		if (value2 <= 96) {
			value2 += 32;
		}
		value2 = (value2 - 115 - (idx * 34));
		value2 = value2 % 26;
		if (value2 < 0) {
			value2 += 26;
		}

		int32_t interimResult = (value1 << 4) | value2;
		int32_t offset = interimResult >= 97 ? 97 : 65;
		interimResult -= offset;
		if (idx == 0) {
			globalValue = 2 + interimResult;
		}
		result[idx] = static_cast<uint8_t>( ((interimResult + globalValue) % 26 + offset) );
// 		size_t idx1 = 17 + idx * 2;
// 		printf("%zu %d %d %d %d %d %d\n", idx1, secret[idx1], value1, value2, interimResult, offset, globalValue);

		globalValue += 3 + interimResult;
	}
	return result;
}

typedef std::vector<std::string> str_list_t;
str_list_t decode_server_list(const std::string& i33628_txt_str)
{
	LOG("i33628_txt_str.length():%zu", i33628_txt_str.length());
	size_t data_size = (i33628_txt_str.length() - 1) / 2;
	std::vector<uint8_t> data;
	data.reserve(data_size);
	data.assign(data_size, 0);
	for (size_t idx = 0; idx < data_size; idx++)
	{
		int32_t u1 = i33628_txt_str[idx * 2 + 1] - 97 - 14 * idx;
		u1 = u1 % 26;
		if (u1 < 0)
			u1 += 26;
		int32_t u2 = i33628_txt_str[idx * 2 + 2] - 104 - 14 * idx;
		u2 = u2 % 26;
		if (u2 < 0)
			u2 += 26;
		data[idx] = (u1 << 4) + u2;
	}
	str_list_t server_list;
	size_t server_list_size = (i33628_txt_str.length() - 1) / 22;
	for (size_t idx = 0; idx < server_list_size; idx++)
	{
		char buf[22];
		snprintf(buf, sizeof(buf), "%d.%d.%d.%d:%d",
			data[idx * 11 + 0],
			data[idx * 11 + 1],
			data[idx * 11 + 2],
			data[idx * 11 + 3],
			(data[idx * 11 + 4] << 16) + (data[idx * 11 + 5] << 8) + data[idx * 11 + 6]
			);
		server_list.push_back(std::string(buf));
	}
	LOG("");
#ifdef DEBUG_LOG
	for (std::string server : server_list)
		printf("%s\n", server.c_str());
#endif  // DEBUG_LOG
	return server_list;
}

#ifdef DECODE_SECRET_TEST
typedef std::array<uint8_t, 24> secret_result_t;
secret_result_t decode_secret1(const void* data, size_t size)
{
	//const uint32_t* secret = reinterpret_cast<const uint32_t*>(data);
	const int8_t* secret = reinterpret_cast<const int8_t*>(data);
	//size_t secret_size = size / sizeof(*secret);
	secret_result_t result{};
	int32_t globalValue = 0;
	for (size_t idx = 0; idx < 24; idx++)
	{
		size_t idx1 = 17 + idx * 2;
		int32_t value1 = secret[17 + idx * 2];
		if (value1 <= 96) {
			value1 += 32;
		}
		value1 = (value1 - 98 - (idx * 34));
		value1 = value1 % 26;
 		//value1 = remainder(value1, 26);
		if (value1 < 0) {
			value1 += 26;
		}

		int32_t value2 = secret[18 + idx * 2];
		if (value2 <= 96) {
			value2 += 32;
		}
		value2 = (value2 - 115 - (idx * 34));
		value2 = value2 % 26;
		//value2 = remainder(value2, 26);
		if (value2 < 0) {
			value2 += 26;
		}

		int32_t interimResult = (value1 << 4) | value2;
		int32_t offset = interimResult >= 97 ? 97 : 65;
		interimResult -= offset;
		if (idx == 0) {
			globalValue = 2 + interimResult;
		}
		result[idx] = static_cast<uint8_t>( ((interimResult + globalValue) % 26 + offset) );
		//result[idx] = static_cast<uint8_t>( remainder((interimResult + globalValue), 26) + offset);
		printf("%zu %d %d %d %d %d %d\n", idx1, secret[idx1], value1, value2, interimResult, offset, globalValue);

		globalValue += 3 + interimResult;
	}
	return result;
}

const char* i33628_txt =
"amvzkluwfelsarzubipgokzhgaianpxcjqypxszgnygjpfeygylnwahownvqxel"
"slhndcwewjlvyfmultovcjdxflbesxggupwdksjrmtahpxcszcqvfblnubiqhpk"
"ryfmzaqxaotdzotszgofnipwdvdyovwiookisqxemdlgnubtaxfmfyjtubhovck"
"bjelszhwudtpwmorynmtaizhcjqximsbrnuklejdkrygxfahovnoqznqvhhryji"
"pwevdyfmtkvoyeufggqvfgnuctbwdkrjimebwahlkwxelsarzubipwjkdzuyfji"
"udcjqypxszgncfibxswdhgsuahownvqxelblgzhclzhkszyfmultovcjrcepfaj"
"xfiqcwdksjrmtahqdcndyhvdgosubiqhpkryfahalsxmoeckvszgofnipwdxtys"
"xcdjudlrqxemdlgnubluwivabhsbkpovckbjelszqtugrsekmwifmtaizhcjqxs"
"zsnoyaeqrjmkrygxfahovgpqemwycopehipwevdyfmtjjoc";
// len - 551

std::string i33628_txt_str(i33628_txt);

int main()
{
	int32_t val = -22 % 26;
	LOG("%d", val);
// 	char secret[] = "  6EULxUGXmjVOPVrFzNIxQdtLLTUdQjaRIZXiyPGxLhCqayOFxlkVQdcMzRNAuKaNjwVHYPpYKbqJGwmDpMyPEbvgHOCwVhBMMUKCukjVPzzHHPNCXjdrGzMhtoLxREdklvhzcLZTMbNkWpfZzHWpFvjgUJgvpzXihUr";
	uint8_t secret[] = {
0x00, 0x00, 0x36, 0x45, 0x55, 0x4c, 0x78, 0x55, 0x47, 0x58,
0x6d, 0x6a, 0x56, 0x4f, 0x50, 0x56, 0x72, 0x46, 0x7a, 0x4e, 0x49, 0x78, 0x51, 0x64, 0x74, 0x4c,
0x4c, 0x54, 0x55, 0x64, 0x51, 0x6a, 0x61, 0x52, 0x49, 0x5a, 0x58, 0x69, 0x79, 0x50, 0x47, 0x78,
0x4c, 0x68, 0x43, 0x71, 0x61, 0x79, 0x4f, 0x46, 0x78, 0x6c, 0x6b, 0x56, 0x51, 0x64, 0x63, 0x4d,
0x7a, 0x52, 0x4e, 0x41, 0x75, 0x4b, 0x61, 0x4e, 0x6a, 0x77, 0x56, 0x48, 0x59, 0x50, 0x70, 0x59,
0x4b, 0x62, 0x71, 0x4a, 0x47, 0x77, 0x6d, 0x44, 0x70, 0x4d, 0x79, 0x50, 0x45, 0x62, 0x76, 0x67,
0x48, 0x4f, 0x43, 0x77, 0x56, 0x68, 0x42, 0x4d, 0x4d, 0x55, 0x4b, 0x43, 0x75, 0x6b, 0x6a, 0x56,
0x50, 0x7a, 0x7a, 0x48, 0x48, 0x50, 0x4e, 0x43, 0x58, 0x6a, 0x64, 0x72, 0x47, 0x7a, 0x4d, 0x68,
0x74, 0x6f, 0x4c, 0x78, 0x52, 0x45, 0x64, 0x6b, 0x6c, 0x76, 0x68, 0x7a, 0x63, 0x4c, 0x5a, 0x54,
0x4d, 0x62, 0x4e, 0x6b, 0x57, 0x70, 0x66, 0x5a, 0x7a, 0x48, 0x57, 0x70, 0x46, 0x76, 0x6a, 0x67,
0x55, 0x4a, 0x67, 0x76, 0x70, 0x7a, 0x58, 0x69, 0x68, 0x55, 0x72
		};
	size_t secret_size = sizeof(secret);
	LOG("secret:%s", secret + 2);
	LOG("secret_size:%zu", secret_size);
	//secret_result_t result = decode_secret1(secret, secret_size);
	secret_result_t result = decode_secret(secret, secret_size);
	LOG("result:%s", result.data());
	LOG("expect:OYiNCSwCIVRXAmeclZlbwHHf");
	decode_server_list(i33628_txt_str);
}

#endif  // DECODE_SECRET_TEST
