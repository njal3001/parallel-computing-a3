#include <fstream>
#include <iostream>
#define OMPI_SKIP_MPICXX 1
#include <mpi.h>

using namespace std;

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using std::string;
using std::vector;

using adjacency_matrix = std::vector<std::vector<size_t>>;

#define GREEN 0
#define YELLOW 1
#define BLUE 2

#define WAITING_PLATFORM 0
#define ON_PLATFORM 1
#define WAITING_TRANSIT 2
#define IN_PLATFORM 3

struct Link {
    int id;
    int from;
    int to;
    int popularity;
    int length;
    int next_link_id[3];
};

struct Troon {
    int id;
    int line;
    int state;
    int state_timestamp;
    int curr_link_id;
};

struct Station {
    int id;
    int forward_station_id[3];
    int backward_station_id[3];
};

void simulate(size_t num_stations,
              const vector<string>& station_names,
              const std::vector<size_t>& popularities,
              const adjacency_matrix& mat,
              const vector<string>& green_station_names,
              const vector<string>& yellow_station_names,
              const vector<string>& blue_station_names, size_t ticks,
              size_t num_green_trains, size_t num_yellow_trains,
              size_t num_blue_trains, size_t num_lines) {
    /**
     * Feel free to delete this printing code, or to wrap it in with #ifdef DEBUG
     * so that `make debug` will build it with the printing code but `make` will
     * not.
     **/

#ifdef DEBUG
    std::cout << num_stations << '\n';

    for (size_t i{}; i < num_stations; ++i) {
        std::cout << station_names[i] << ' ' << popularities[i] << ' ';
    }
    std::cout << '\n';

    for (size_t i{}; i < num_stations; ++i) {
        for (size_t j{}; j < num_stations; ++j) {
            std::cout << mat[i][j] << ' ';
        }
        std::cout << '\n';
    }

    for (const auto& stn : green_station_names) {
        std::cout << stn << ' ';
    }
    std::cout << '\n';

    for (const auto& stn : yellow_station_names) {
        std::cout << stn << ' ';
    }
    std::cout << '\n';

    for (const auto& stn : blue_station_names) {
        std::cout << stn << ' ';
    }
    std::cout << '\n';

    std::cout << ticks << '\n';
    std::cout << num_green_trains << '\n';
    std::cout << num_yellow_trains << '\n';
    std::cout << num_blue_trains << '\n';

    std::cout << num_lines << '\n';
#endif
    unordered_map<string, int> station_to_id{};
    for (int i = 0; i < num_stations; i++) {
        station_to_id.insert(make_pair(station_names[i], i));
    }

    Station* stations;
    stations = (Station*)malloc(sizeof(Station) * num_stations);

    for (int i = 0; i < num_stations; i++) {
        auto station = stations + i;
        station->id = i;
        for (size_t j = 0; j < 3; j++) {
            station->backward_station_id[j] = -1;
            station->forward_station_id[j] = -1;
        }
    }
    for (size_t i = 0; i < green_station_names.size() - 1; i++) {
        auto station = stations + station_to_id[green_station_names[i]];
        auto next_station = stations + station_to_id[green_station_names[i + 1]];
        station->forward_station_id[GREEN] = next_station->id;
        next_station->backward_station_id[GREEN] = station->id;
    }
    for (size_t i = 0; i < yellow_station_names.size() - 1; i++) {
        auto station = stations + station_to_id[yellow_station_names[i]];
        auto next_station = stations + station_to_id[yellow_station_names[i + 1]];
        station->forward_station_id[YELLOW] = next_station->id;
        next_station->backward_station_id[YELLOW] = station->id;
    }
    for (size_t i = 0; i < blue_station_names.size() - 1; i++) {
        auto station = stations + station_to_id[blue_station_names[i]];
        auto next_station = stations + station_to_id[blue_station_names[i + 1]];
        station->forward_station_id[BLUE] = next_station->id;
        next_station->backward_station_id[BLUE] = station->id;
    }

    int num_links = 0;
    for (size_t i = 0; i < num_stations; i++) {
        auto station = stations + i;
        for (size_t j = 0; j < num_stations; j++) {
            auto next_station = stations + j;
            if (mat[i][j] > 0) {
                num_links++;
            }
        }
    }
    auto connections = (int*)malloc(sizeof(int) * num_stations * num_stations);
    auto links = (Link*)malloc(sizeof(Link) * num_links);
    int link_cnt = 0;
    for (size_t i = 0; i < num_stations; i++) {
        for (size_t j = 0; j < num_stations; j++) {
            if (mat[i][j] > 0) {
                auto link = links + link_cnt;
                link->id = link_cnt;
                link->length = mat[i][j];
                link->popularity = popularities[i];
                link->from = i;
                link->to = j;
                for (size_t k = 0; k < 3; k++) {
                    link->next_link_id[k] = -1;
                }
                connections[i * num_stations + j] = link_cnt;
                link_cnt++;
            }
        }
    }

    // Connect links
    vector<string> all_line_names[3] = {green_station_names, yellow_station_names, blue_station_names};
    int first_link_forward_idx[3] = {-1, -1, -1};
    int first_link_backward_idx[3] = {-1, -1, -1};
    int last_link_backward_idx[3] = {-1, -1, -1};
    for (int line_idx = 0; line_idx < 3; line_idx++) {
        auto line_names = all_line_names[line_idx];
        auto prev_link_idx = -1;
        for (size_t i = 0; i < line_names.size() - 1; i++) {
            auto curr_link_idx = connections[station_to_id[line_names[i]] * num_stations + station_to_id[line_names[i + 1]]];
            if (prev_link_idx != -1) {
                links[prev_link_idx].next_link_id[line_idx] = curr_link_idx;
            } else {
                first_link_forward_idx[line_idx] = curr_link_idx;
            }
            prev_link_idx = curr_link_idx;
        }
        for (int i = line_names.size() - 1; i > 0; i--) {
            auto curr_link_idx = connections[station_to_id[line_names[i]] * num_stations + station_to_id[line_names[i - 1]]];
            links[prev_link_idx].next_link_id[line_idx] = curr_link_idx;
            if (i == 1) {
                last_link_backward_idx[line_idx] = curr_link_idx;
            }
            if (i == line_names.size() - 1) {
                first_link_backward_idx[line_idx] = curr_link_idx;
            }
            prev_link_idx = curr_link_idx;
        }
        links[last_link_backward_idx[line_idx]].next_link_id[line_idx] = first_link_forward_idx[line_idx];
    }
    free(connections);

    int num_troons[3] = {(int)num_green_trains, num_yellow_trains, num_blue_trains};

    MPI_Init(NULL, NULL);

    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Each process handles a subset of links
    int link_group_size = (num_links + size - 1) / size;
    int my_links_start = min(rank * link_group_size, num_links);
    int my_links_end = min(my_links_start + link_group_size, num_links);
    int my_links_count = my_links_end - my_links_start;
    for (int tick = 0; tick < ticks; tick++) {
        // Process 0 will create all troons and send it to corresponding process handling the first link.
        if (rank == 0) {
            printf("Number of links: %d\n", num_links);
        }
    }

    MPI_Finalize();

    free(stations);
    free(links);
}
/* for (int i = 0; i < num_stations; i++) {
            cout << stations[i].id << " " << station_names[stations[i].id] << " green_forward: " << station_names[stations[i].forward_station_id[GREEN]] << " green_backward: " << station_names[stations[i].backward_station_id[GREEN]] << " yellow_forward: " << station_names[stations[i].forward_station_id[YELLOW]] << " yellow_backward: " << station_names[stations[i].backward_station_id[YELLOW]] << " blue_forward: " << station_names[stations[i].forward_station_id[BLUE]] << " blue_backward: " << station_names[stations[i].backward_station_id[BLUE]] << endl;
        } */
/* for (int i = 0; i < num_links; i++) {
    auto link = links + i;
    auto next_link = links + link->next_link_id[BLUE];
    if (link->next_link_id[BLUE] != -1) {
        cout << link->id << ": " << station_names[link->from] << "->" << station_names[link->to] << " to " << station_names[next_link->from] << "->" << station_names[next_link->to] << endl;
    }
}
 */
vector<string> extract_station_names(string& line) {
    constexpr char space_delimiter = ' ';
    vector<string> stations{};
    line += ' ';
    size_t pos;
    while ((pos = line.find(space_delimiter)) != string::npos) {
        stations.push_back(line.substr(0, pos));
        line.erase(0, pos + 1);
    }
    return stations;
}

int main(int argc, char const* argv[]) {
    using std::cout;

    if (argc < 2) {
        std::cerr << argv[0] << " <input_file>\n";
        std::exit(1);
    }

    std::ifstream ifs(argv[1], std::ios_base::in);
    if (!ifs.is_open()) {
        std::cerr << "Failed to open " << argv[1] << '\n';
        std::exit(2);
    }

    // Read S
    size_t S;
    ifs >> S;

    // Read station names.
    string station;
    std::vector<string> station_names{};
    station_names.reserve(S);
    for (size_t i = 0; i < S; ++i) {
        ifs >> station;
        station_names.emplace_back(station);
    }

    // Read P popularity
    size_t p;
    std::vector<size_t> popularities{};
    popularities.reserve(S);
    for (size_t i = 0; i < S; ++i) {
        ifs >> p;
        popularities.emplace_back(p);
    }

    // Form adjacency mat
    adjacency_matrix mat(S, std::vector<size_t>(S));
    for (size_t src{}; src < S; ++src) {
        for (size_t dst{}; dst < S; ++dst) {
            ifs >> mat[src][dst];
        }
    }

    ifs.ignore();

    string stations_buf;

    std::getline(ifs, stations_buf);
    auto green_station_names = extract_station_names(stations_buf);

    std::getline(ifs, stations_buf);
    auto yellow_station_names = extract_station_names(stations_buf);

    std::getline(ifs, stations_buf);
    auto blue_station_names = extract_station_names(stations_buf);

    // N time ticks
    size_t N;
    ifs >> N;

    // g,y,b number of trains per line
    size_t g, y, b;
    ifs >> g;
    ifs >> y;
    ifs >> b;

    size_t num_lines;
    ifs >> num_lines;

    simulate(S, station_names, popularities, mat, green_station_names,
             yellow_station_names, blue_station_names, N, g, y, b, num_lines);

    return 0;
}
