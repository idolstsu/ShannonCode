#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <chrono>
#include <cstdint>
#include <locale>
#include <cmath>

using namespace std;
using namespace std::chrono;

const uint32_t MAX_FREQ = 16383;
const uint32_t TOP = 0xFFFF;
const uint32_t FIRST_QTR = (TOP + 1) / 4;
const uint32_t HALF = 2 * FIRST_QTR;
const uint32_t THIRD_QTR = 3 * FIRST_QTR;

struct SymbolRange {
    uint32_t low;
    uint32_t high;
    uint32_t count;
};

map<char, uint32_t> calculateFrequencies(const string& text) {
    map<char, uint32_t> freq;
    for (char current_char : text) {
        if (freq[current_char] < MAX_FREQ) freq[current_char]++;
    }
    return freq;
}

double calculateShannonEntropy(const map<char, uint32_t>& freq, uint32_t total) {
    double entropy = 0.0;
    for (const auto& pair : freq) {
        double pi = static_cast<double>(pair.second) / total;
        if (pi > 0) {
            entropy -= pi * log2(pi);
        }
    }
    return entropy;
}

void buildCumulativeFreq(const map<char, uint32_t>& freq, map<char, SymbolRange>& ranges, uint32_t& total) {
    uint32_t cumulative = 0;
    total = 0;
    for (const auto& pair : freq) total += pair.second;
    for (const auto& pair : freq) {
        ranges[pair.first] = { cumulative, cumulative + pair.second, total };
        cumulative += pair.second;
    }
}

void writeBitVector(ofstream& out, const vector<bool>& bits) {
    uint32_t bitSize = bits.size();
    out.write(reinterpret_cast<const char*>(&bitSize), sizeof(bitSize));

    uint8_t byte = 0;
    int bit_count = 0;
    for (bool bit : bits) {
        byte = (byte << 1) | (bit ? 1 : 0);
        bit_count++;
        if (bit_count == 8) {
            out.put(static_cast<char>(byte));
            byte = 0;
            bit_count = 0;
        }
    }
    if (bit_count > 0) {
        byte <<= (8 - bit_count);
        out.put(static_cast<char>(byte));
    }
}

vector<bool> readBitVector(ifstream& in) {
    uint32_t bitSize;
    in.read(reinterpret_cast<char*>(&bitSize), sizeof(bitSize));
    vector<bool> bits;
    bits.reserve(bitSize);
    char byte;
    while (in.get(byte) && bits.size() < bitSize) {
        for (int i = 7; i >= 0 && bits.size() < bitSize; i--) {
            bits.push_back((byte >> i) & 1);
        }
    }
    return bits;
}

void compressFile(ifstream& in, ofstream& out) {
    auto start = high_resolution_clock::now();

    string text((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
    size_t original_size = text.size();

    map<char, uint32_t> freq = calculateFrequencies(text);
    map<char, SymbolRange> ranges;
    uint32_t total = 0;
    buildCumulativeFreq(freq, ranges, total);

    double entropy = calculateShannonEntropy(freq, total);
    cout << "Shannon Entropy: " << entropy << " bits per symbol\n";

    uint32_t low = 0;
    uint32_t high = TOP;
    uint32_t pending_bits = 0;
    vector<bool> output_bits;

    for (char current_char : text) {
        uint32_t range = high - low + 1;
        const SymbolRange& current_range = ranges[current_char];

        high = low + (range * current_range.high) / current_range.count - 1;
        low = low + (range * current_range.low) / current_range.count;

        while (true) {
            if (high < HALF) {
                output_bits.push_back(false);
                while (pending_bits > 0) {
                    output_bits.push_back(true);
                    pending_bits--;
                }
                low <<= 1;
                high = (high << 1) | 1;
            }
            else if (low >= HALF) {
                output_bits.push_back(true);
                while (pending_bits > 0) {
                    output_bits.push_back(false);
                    pending_bits--;
                }
                low = (low - HALF) << 1;
                high = ((high - HALF) << 1) | 1;
            }
            else if (low >= FIRST_QTR && high < THIRD_QTR) {
                pending_bits++;
                low = (low - FIRST_QTR) << 1;
                high = ((high - FIRST_QTR) << 1) | 1;
            }
            else break;
        }
    }

    pending_bits++;
    if (low < FIRST_QTR) {
        output_bits.push_back(false);
        while (pending_bits-- > 0) output_bits.push_back(true);
    }
    else {
        output_bits.push_back(true);
        while (pending_bits-- > 0) output_bits.push_back(false);
    }

    uint32_t freq_size = static_cast<uint32_t>(freq.size());
    out.write(reinterpret_cast<const char*>(&freq_size), sizeof(freq_size));
    for (const auto& pair : freq) {
        out.put(pair.first);
        out.write(reinterpret_cast<const char*>(&pair.second), sizeof(pair.second));
    }

    uint32_t text_size = static_cast<uint32_t>(text.size());
    out.write(reinterpret_cast<const char*>(&text_size), sizeof(text_size));

    writeBitVector(out, output_bits);

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start);
    size_t compressed_size = (output_bits.size() + 7) / 8;

    cout << "\nCompression results:\n";
    cout << "Original size: " << original_size << " bytes\n";
    cout << "Compressed size: " << compressed_size << " bytes\n";
    cout << "Compression ratio: "
        << (1.0 - (double)compressed_size / original_size) * 100
        << "%\n";
    cout << "Time taken: " << duration.count() << " ms\n";
}

void decompressFile(ifstream& in, ofstream& out) {
    auto start = high_resolution_clock::now();

    uint32_t freq_size;
    in.read(reinterpret_cast<char*>(&freq_size), sizeof(freq_size));

    map<char, uint32_t> freq;
    for (uint32_t i = 0; i < freq_size; i++) {
        char current_char;
        uint32_t frequency;
        in.get(current_char);
        in.read(reinterpret_cast<char*>(&frequency), sizeof(frequency));
        freq[current_char] = frequency;
    }

    uint32_t text_size;
    in.read(reinterpret_cast<char*>(&text_size), sizeof(text_size));

    vector<bool> bits = readBitVector(in);
    map<char, SymbolRange> ranges;
    uint32_t total = 0;
    buildCumulativeFreq(freq, ranges, total);

    string result;
    result.reserve(text_size);

    uint32_t value = 0;
    size_t bit_index = 0;
    for (int i = 0; i < 16 && bit_index < bits.size(); i++) {
        value = (value << 1) | (bits[bit_index++] ? 1 : 0);
    }

    uint32_t low = 0, high = TOP;

    for (uint32_t i = 0; i < text_size; i++) {
        uint32_t range = high - low + 1;
        uint32_t scaled_value = ((value - low + 1) * total - 1) / range;

        char symbol = 0;
        for (const auto& pair : ranges) {
            const SymbolRange& current_range = pair.second;
            if (scaled_value >= current_range.low && scaled_value < current_range.high) {
                symbol = pair.first;
                break;
            }
        }

        result += symbol;
        const SymbolRange& current_range = ranges[symbol];
        high = low + (range * current_range.high) / current_range.count - 1;
        low = low + (range * current_range.low) / current_range.count;

        while (true) {
            if (high < HALF) {
                low <<= 1;
                high = (high << 1) | 1;
                value <<= 1;
            }
            else if (low >= HALF) {
                low = (low - HALF) << 1;
                high = ((high - HALF) << 1) | 1;
                value = (value - HALF) << 1;
            }
            else if (low >= FIRST_QTR && high < THIRD_QTR) {
                low = (low - FIRST_QTR) << 1;
                high = ((high - FIRST_QTR) << 1) | 1;
                value = (value - FIRST_QTR) << 1;
            }
            else break;

            if (bit_index < bits.size()) {
                value |= (bits[bit_index++] ? 1 : 0);
            }
        }
    }

    out.write(result.data(), result.size());

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start);

    cout << "\nDecompression results:\n";
    cout << "Decompressed size: " << text_size << " bytes\n";
    cout << "Time taken: " << duration.count() << " ms\n";
}

int main() {
    setlocale(LC_ALL, "Russian");

    string filename;
    cout << "Enter filename (exp.txt to compress | encoded.txt to decompress): ";
    cin >> filename;

    ifstream inFile(filename, ios::binary);
    if (!inFile) {
        cout << "Error opening file!" << endl;
        return 1;
    }

    string choice;
    cout << "Enter '1' to compress or '2' to decompress: ";
    cin >> choice;

    if (choice == "1") {
        ofstream outFile("encoded.txt", ios::binary);
        if (!outFile) {
            cout << "Error creating output file!" << endl;
            return 1;
        }

        cout << "Compressing..." << endl;
        compressFile(inFile, outFile);
    }
    else if (choice == "2") {
        ofstream outFile("decoded.txt", ios::binary);
        if (!outFile) {
            cout << "Error creating output file!" << endl;
            return 1;
        }

        cout << "Decompressing..." << endl;
        decompressFile(inFile, outFile);
    }
    else {
        cout << "Invalid choice!" << endl;
        return 1;
    }

    inFile.close();
    return 0;
}