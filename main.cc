#include <fstream>
#include <iostream>
#define OMPI_SKIP_MPICXX 1
#include <mpi.h>
#include <string>
#include <vector>

using std::string;
using std::vector;

using adjacency_matrix = std::vector<std::vector<size_t>>;

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

  // #ifdef DEBUG
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
  // #endif
}

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
