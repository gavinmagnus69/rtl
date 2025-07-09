#ifndef rtl_request_h
#define rtl_request_h


#include <format>
#include <numbers>
#include <set>


namespace rtl {
namespace Request {

constexpr enum MessageType { testStruct, messageStruct, responseStruct };

// looks like standard std container behavior
struct IBaseMessage {
    virtual MessageType type() const = 0;
    virtual size_t size() const = 0;
    virtual const char* data() const = 0;
    virtual char* raw() = 0;
    virtual std::set<int32_t> alignedBytes() const = 0;
};

// aligned for float(4 bytes)
struct MessageStruct : IBaseMessage {
    struct Data {
        bool calibration_start = true;      // 0
        bool zero_positioning_start = true; // 1
        bool measuring_start = false;       // 2
        bool abort_measure = true;          // 3
        bool fault_reset = false;           // 4
        bool save_measurements = true;      // 5
        bool new_data_transfer = true;      // 6
        // one byte alining //7
        float scan_length = 101.228;      // 8-11
        float calibration_diameter = 200; // 12-15
    };
    Data m_data;
    MessageType type() const override {
        return MessageType::messageStruct;
    }
    size_t size() const override {
        return sizeof(m_data);
    }
    const char* data() const override {
        return reinterpret_cast<const char*>(&m_data);
    }
    char* raw() {
        return reinterpret_cast<char*>(&m_data);
    }
    std::set<int32_t> alignedBytes() const override {
        return {7};
    }
    std::string toString() const {
        return std::format("{} {} {} {} {} {} {} {} {}", m_data.calibration_start, m_data.zero_positioning_start, m_data.measuring_start, m_data.abort_measure, m_data.fault_reset,
                           m_data.save_measurements, m_data.new_data_transfer, m_data.scan_length, m_data.calibration_diameter);
    }
    MessageStruct::Data& fields() {
        return m_data;
    }
};


struct ResponseStruct : IBaseMessage {

    struct Data {
        bool calibration_running = false;         // 1
        bool calibration_completed = false;       // 2
        bool calibration_error = false;           // 3
        bool zero_positioning_running = false;    // 4
        bool zero_positioning_completed = false;  // 5
        bool zero_positioning_error = false;      // 6
        bool measuring_running = false;           // 7
        bool measuring_error = false;             // 8
        bool measuring_completed = false;         // 9
        bool measuring_aborted = false;           // 10
        bool system_in_fault_condition = false;   // 11
        bool save_measurements_running = false;   // 12
        bool save_measurements_completed = false; // 13
        bool save_measurements_error = false;     // 14
        bool data_transfer_read = false;          // 15
        // aligned byte
        float diameter_min = 0;                // 19
        float diameter_max = 0;                // 23
        float diameter_avg = 0;                // 27
        float chamfer_width_min = 0;           // 31
        float chamfer_width_max = 0;           // 35
        float chamfer_width_avg = 0;           // 39
        float circularity_min = 0;             // 43
        float circularity_max = 0;             // 47
        float circularity_avg = 0;             // 51
        float bevel_level_of_chamfers_min = 0; // 55
        float bevel_level_of_chamfers_max = 0; // 59
        float bevel_level_of_chamfers_avg = 0; // 63
        float band_height_min = 0;             // 67
        float band_height_max = 0;             // 71
        float band_height_avg = 0;             // 75


        std::string toStringInner() const {
            // small helper to append "name: value\n" lines
            std::string buf;
            buf.reserve(1024);
            auto append = [&](std::string_view name, const auto& val) {
                // adjust width (30) as you like for alignment
                buf += std::format("  {:<30}: {}\n", name, val);
            };


            buf += "Data {\n";

            append("calibration_running", calibration_running);
            append("calibration_completed", calibration_completed);
            append("calibration_error", calibration_error);
            append("zero_positioning_running", zero_positioning_running);
            append("zero_positioning_completed", zero_positioning_completed);
            append("zero_positioning_error", zero_positioning_error);
            append("measuring_running", measuring_running);
            append("measuring_error", measuring_error);
            append("measuring_completed", measuring_completed);
            append("measuring_aborted", measuring_aborted);
            append("system_in_fault_condition", system_in_fault_condition);
            append("save_measurements_running", save_measurements_running);
            append("save_measurements_completed", save_measurements_completed);
            append("save_measurements_error", save_measurements_error);
            append("data_transfer_read", data_transfer_read);

            append("diameter_min", diameter_min);
            append("diameter_max", diameter_max);
            append("diameter_avg", diameter_avg);
            append("chamfer_width_min", chamfer_width_min);
            append("chamfer_width_max", chamfer_width_max);
            append("chamfer_width_avg", chamfer_width_avg);
            append("circularity_min", circularity_min);
            append("circularity_max", circularity_max);
            append("circularity_avg", circularity_avg);
            append("bevel_level_of_chamfers_min", bevel_level_of_chamfers_min);
            append("bevel_level_of_chamfers_max", bevel_level_of_chamfers_max);
            append("bevel_level_of_chamfers_avg", bevel_level_of_chamfers_avg);
            append("band_height_min", band_height_min);
            append("band_height_max", band_height_max);
            append("band_height_avg", band_height_avg);

            buf += "}";
            return buf;
        }
    };
    Data m_data;
    MessageType type() const override {
        return MessageType::responseStruct;
    }
    size_t size() const override {
        return sizeof(m_data);
    }
    const char* data() const override {
        return reinterpret_cast<const char*>(&m_data);
    }
    char* raw() {
        return reinterpret_cast<char*>(&m_data);
    }
    std::string toString() const {
        return "bruh\n";
    }
    std::set<int32_t> alignedBytes() const override {
        return {16};
    }
    ResponseStruct::Data& fields() {
        return m_data;
    }
    std::string toString() {
        return m_data.toStringInner();
    }
};


}; // namespace Request
}; // namespace rtl
#endif


// struct Data {
//     float measurement_height = 0; //4
//     float diameter = 0; //8
//     float circularity = 0; //12
//     uint32_t measurements_by_height = 0; //16
//     uint32_t measurements_by_corner = 0; //20
//     float angle = 0; //24
//     float width_top_chamfer = 0; //28
//     float width_bottom_chamfer = 0; //32
//     float top_corner = 0; //36
//     float bottom_corner = 0; //40
//     float height_measured_part = 0; //44
//     float diameter_min = 0; //48
//     float diameter_max = 0; //52
//     float diameter_avg = 0; //56
//     float chamfer_width_min = 0; //60
//     float chamfer_width_max = 0; //64
//     float chamfer_width_avg = 0; //68
//     float circularity_min = 0; //72
//     float circularity_max = 0; //76
//     float circularity_avg = 0; //80
//     float bevel_level_of_chamfers_min = 0; //84
//     float bevel_level_of_chamfers_max = 0; //88
//     float bevel_level_of_chamfers_avg = 0; //92
//     bool calibration_running = false; //93
//     bool calibration_completed = false; //94
//     bool calibration_error = false; //95
//     bool zero_positioning_running = false; //96
//     bool zero_positioning_completed = false; //97
//     bool zero_positioning_error = false; //98
//     bool measuring_running = false; //99
//     bool measuring_error = false; //100
//     bool measuring_completed = false; //101
//     bool measuring_aborted = false; //102
//     bool system_in_fault_condition = false; //103
//     bool data_transfer_read = false; //104
// };