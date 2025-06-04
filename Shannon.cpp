#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>
#include <string>
#include <limits>
#include <cctype>

using namespace std;

struct SymbolInfo {
    unsigned char symbol;
    double probability;
    string code;
};

map<unsigned char, double> calculateProbabilities(const string& data) {
    map<unsigned char, int> freq;
    for (unsigned char c : data) freq[c]++;

    map<unsigned char, double> prob;
    int total = data.size();
    for (const auto& pair : freq) {
        prob[pair.first] = static_cast<double>(pair.second) / total;
    }
    return prob;
}

vector<SymbolInfo> buildShannonCodes(const map<unsigned char, double>& probabilities) {
    vector<SymbolInfo> symbols;
    for (const auto& pair : probabilities) {
        symbols.push_back({ pair.first, pair.second, "" });
    }

    sort(symbols.begin(), symbols.end(), [](const SymbolInfo& a, const SymbolInfo& b) {
        return a.probability > b.probability;
        });

    double sum = 0.0;
    for (auto& symbol : symbols) {
        int code_length = ceil(log2(1.0 / symbol.probability));
        double q = sum;
        sum += symbol.probability;

        string code;
        for (int i = 0; i < code_length; ++i) {
            q *= 2;
            code += (q >= 1.0) ? '1' : '0';
            if (q >= 1.0) q -= 1.0;
        }
        symbol.code = code;
    }
    return symbols;
}

void encodeFile(const string& inputFile, const string& outputFile) {
    ifstream in(inputFile, ios::binary);
    if (!in) {
        cerr << "Error: Cannot open input file!" << endl;
        return;
    }

    string data((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
    in.close();

    auto probabilities = calculateProbabilities(data);
    auto codes = buildShannonCodes(probabilities);
    map<unsigned char, string> code_map;
    for (const auto& info : codes) {
        code_map[info.symbol] = info.code;
    }

    ofstream out(outputFile, ios::binary);
    if (!out) {
        cerr << "Error: Cannot create output file!" << endl;
        return;
    }

    // Write header: symbol count (1 byte)
    size_t symbol_count = codes.size();
    out.write(reinterpret_cast<const char*>(&symbol_count), sizeof(symbol_count));

    // Write code table efficiently
    for (const auto& info : codes) {
        out.put(info.symbol);
        uint8_t code_length = info.code.size();
        out.put(code_length);

        // Pack code bits into bytes
        unsigned char buffer = 0;
        int bit_pos = 0;
        for (char bit : info.code) {
            if (bit == '1') {
                buffer |= (1 << (7 - bit_pos));
            }
            bit_pos++;

            if (bit_pos == 8) {
                out.put(buffer);
                buffer = 0;
                bit_pos = 0;
            }
        }
        // Write remaining bits if any
        if (bit_pos > 0) {
            out.put(buffer);
        }
    }

    // Encode data and pack bits into bytes
    unsigned char buffer = 0;
    int bit_pos = 0;
    for (unsigned char c : data) {
        const string& code = code_map[c];
        for (char bit : code) {
            if (bit == '1') {
                buffer |= (1 << (7 - bit_pos));
            }
            bit_pos++;

            if (bit_pos == 8) {
                out.put(buffer);
                buffer = 0;
                bit_pos = 0;
            }
        }
    }

    // Write remaining bits if any
    if (bit_pos > 0) {
        out.put(buffer);
    }

    out.close();

    // Calculate compression ratio
    ifstream orig(inputFile, ios::ate | ios::binary);
    ifstream comp(outputFile, ios::ate | ios::binary);
    size_t original_size = orig.tellg();
    size_t compressed_size = comp.tellg();
    orig.close();
    comp.close();

    cout << "File successfully encoded." << endl;
    cout << "Original size: " << original_size << " bytes" << endl;
    cout << "Compressed size: " << compressed_size << " bytes" << endl;
    cout << "Compression ratio: " << (compressed_size * 100 / original_size) << "%" << endl;
}

void decodeFile(const string& inputFile, const string& outputFile) {
    ifstream in(inputFile, ios::binary);
    if (!in) {
        cerr << "Error: Cannot open input file!" << endl;
        return;
    }

    // Read header
    size_t symbol_count;
    in.read(reinterpret_cast<char*>(&symbol_count), sizeof(symbol_count));

    // Read code table
    vector<SymbolInfo> codes;
    map<string, unsigned char> decode_map;
    for (size_t i = 0; i < symbol_count; ++i) {
        SymbolInfo info;
        info.symbol = in.get();
        uint8_t code_length = in.get();

        // Read packed code bits
        string code_str;
        int bits_read = 0;
        while (bits_read < code_length) {
            unsigned char byte = in.get();
            for (int j = 7; j >= 0 && bits_read < code_length; j--, bits_read++) {
                if (byte & (1 << j)) {
                    code_str += '1';
                }
                else {
                    code_str += '0';
                }
            }
        }
        info.code = code_str;
        decode_map[code_str] = info.symbol;
    }

    // Decode data
    ofstream out(outputFile, ios::binary);
    if (!out) {
        cerr << "Error: Cannot create output file!" << endl;
        return;
    }

    string current_code;
    unsigned char byte;
    while (in.get(reinterpret_cast<char&>(byte))) {
        for (int i = 7; i >= 0; --i) {
            char bit = (byte & (1 << i)) ? '1' : '0';
            current_code += bit;

            auto it = decode_map.find(current_code);
            if (it != decode_map.end()) {
                out.put(it->second);
                current_code.clear();
            }
        }
    }

    out.close();
    cout << "File successfully decoded." << endl;
}

int main() {
    cout << "Enter '1' to compress or '2' to decompress: ";

    int choice;
    cin >> choice;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    cout << "Enter filename (exp.txt to compress | encode.txt to decompress): ";
    string filename;
    getline(cin, filename);

    if (choice == 1) {
        string output = "encode.txt";
        encodeFile(filename, output);
    }
    else if (choice == 2) {
        string output = "decode.txt";
        decodeFile(filename, output);
    }
    else {
        cerr << "Error: Invalid choice!" << endl;
        return 1;
    }

    return 0;
}
