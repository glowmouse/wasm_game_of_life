///
/// Game of life programmed with Web Assembly
/// (C) Andrew Brownbill 2019
///  

#include <iostream>
#include <utility>
#include <unordered_map>
#include <vector>
#include <string>

#include <SDL/SDL.h>
#include <emscripten.h>

// X and Y screen resolution
constexpr int X_SCREEN=1024;
constexpr int Y_SCREEN=768;

// For bigger game of life cells
constexpr int PIXEL_PER_GRID=2;

// Create the game of life play board.
constexpr int X_GRID=X_SCREEN/PIXEL_PER_GRID;
constexpr int Y_GRID=Y_SCREEN/PIXEL_PER_GRID;

// Game of life co-ordinate.  X = first, Y = second.
using LifeCoord = std::pair<unsigned,unsigned>;

// For making patterns using ASCII art.
using Pattern = std::vector< std::string >;

// A Glider Gun
const Pattern gliderGun = {
  "                         X             ",
  "                       X X             ",
  "             XX      XX            XX  ",
  "            X   X    XX            XX  ",
  " XX        X     X   XX                ",
  " XX        X   X XX    X X             ",
  "           X     X       X             ",
  "            X   X                      ",
  "             XX                        ",
};

// Hash function for a game of life coordinate.
namespace std {
  template<>
  struct hash<LifeCoord>
  {
    std::size_t operator()(const LifeCoord& k) const {
      return hash<unsigned>()(k.first ^ (k.second << 16));
    }
  };
}

// A simple game of life cell state. Defaults to 0
class CellState
{
  public:

  CellState(void) { value = 0; }
  unsigned value;
};

// The game of life buffer.  Maps coordinates to cell states.
using LifeBuffer = std::unordered_map<LifeCoord,CellState>;

// We need a double buffer to build the next state.
using LifeDBuffer = std::pair<LifeBuffer,LifeBuffer>;

// Make a Color palette
class Palette
{
  public:

  Palette( const SDL_Surface* screen ) 
  {
    // Number of color palette entries
    constexpr unsigned NUM_COLORS=256;

    // Interpolate the palette using 3 base colors.
    constexpr unsigned BASE_COLORS=3;
    constexpr unsigned COLOR_RANGES = BASE_COLORS-1;
    constexpr unsigned ENTRIES_PER_RANGE =    // Round up 
        ( NUM_COLORS + COLOR_RANGES-1) / ( COLOR_RANGES );
    constexpr unsigned RI = 0;    // Red Index
    constexpr unsigned GI = 1;    // Green Index
    constexpr unsigned BI = 2;    // Blue Index

    const unsigned int col[ BASE_COLORS ][3] = { 
      { 128, 220, 255 },    // Light Blue  
      { 255, 255, 0  },     // Yellow
      { 255, 0,  0   }};    // Red

    for ( int i = 0; i < NUM_COLORS ; ++i )
    {
      const unsigned int cn = i / ENTRIES_PER_RANGE;
      const unsigned int co = cn + 1;
      const unsigned int s = i % ENTRIES_PER_RANGE;
      const unsigned int oms = ENTRIES_PER_RANGE - s;

      const unsigned r = col[co][RI] * s   / ENTRIES_PER_RANGE + 
                         col[cn][RI] * oms / ENTRIES_PER_RANGE;
      const unsigned g = col[co][GI] * s   / ENTRIES_PER_RANGE +
                         col[cn][GI] * oms / ENTRIES_PER_RANGE;
      const unsigned b = col[co][BI] * s   / ENTRIES_PER_RANGE +
                         col[cn][BI] * oms / ENTRIES_PER_RANGE;

      values.push_back( SDL_MapRGBA( screen->format, r, g, b, 255 ));
    }
  }
  Palette() = delete;
  Palette( const Palette& ) = delete;
  Palette& operator=( const Palette& ) = delete;

  std::vector<Uint32> values;
};

// Draw the game of life buffer on the screen.
void drawScreen( SDL_Surface *screen, const LifeBuffer& buffer, const LifeBuffer& age )
{
  const Uint32 black = SDL_MapRGBA( screen->format, 0, 0, 0, 255 );
  const Palette palette(screen);

  // Clear
  Uint32 *start = (Uint32*)screen->pixels;
  Uint32 *end= start + X_SCREEN * Y_SCREEN;
  for ( Uint32 *cur = start; cur < end; ++cur ) *cur = black;

  // Update with set cells
  for ( const auto& i : buffer )
  {
    const LifeCoord& c = i.first;
    constexpr unsigned AGE_RATE = 16;   // lower values = faster color aging
    const unsigned int adjustedAge = age.at( c ).value / AGE_RATE;
    const unsigned int clippedAge = (adjustedAge < palette.values.size()) ?
        adjustedAge : palette.values.size()-1;

    if ( i.second.value )
    {
      for ( int x = 0; x < PIXEL_PER_GRID; ++x ) {
        for ( int y = 0; y < PIXEL_PER_GRID; ++y ) {
          const int xc = x + c.first  * PIXEL_PER_GRID;
          const int yc = y + c.second * PIXEL_PER_GRID;
          start[ xc + yc * X_SCREEN ] = palette.values.at( clippedAge );
        }
      }
    }
  }
}

// Move the game forward one iteration.
void advanceSim( LifeDBuffer &dbuffer )
{
  // Swap old for new.
  std::swap( dbuffer.first, dbuffer.second );

  // Clear out the new buffer.
  dbuffer.first.clear();

  // Figure out hold many neighbors each cell has.
  for ( const auto& i : dbuffer.second )
  {
    if ( i.second.value == 0 ) continue;
    const LifeCoord& c = i.first;
    for ( int x = -1; x <=1; ++x ) {
      for ( int y = -1; y <=1; ++y ) {
        if ( x !=0 || y != 0 ) {    // I can't be a neighbor of myself
          const int xc = (x+c.first  + X_GRID) % X_GRID; // wrap around x
          const int yc = (y+c.second + Y_GRID) % Y_GRID; // wrap around y
          dbuffer.first[ LifeCoord( xc, yc )].value += 1;
        }
      }
    }   
  }

  // Apply the game of life rules to any cells with neighbors from
  // the old buffer.
  for ( auto& i : dbuffer.first )
  {
    if ( i.second.value <= 1 ) i.second.value = 0;         // Starve
    else if ( i.second.value == 3 ) i.second.value = 1;    // Expand 
    else if ( i.second.value >= 4 ) i.second.value = 0;    // Overpopulate
    else i.second.value = dbuffer.second[ i.first ].value; // Same as before
  }
}

void advanceAge( LifeBuffer& age, const LifeBuffer& current )
{
  std::vector< LifeCoord > toErase;   // Erase all cells not in current
  for ( auto& cell : age )
  {
    if ( current.count( cell.first ) == 0 ) toErase.push_back( cell.first );
  }
  for ( const auto& key : toErase ) age.erase( key );

  for ( const auto& cell : current )
  {
    age[ cell.first ].value += 1;
  }
} 

void dropPattern( 
  LifeBuffer& grid,           // Destination 
  const unsigned x,           // x target location
  const unsigned y,           // y target location
  const Pattern& pattern,     // The pattern to write to that location
  const unsigned int rotate ) // How should the pattern be rotated? (0-3).
{
  int yp=0;
  for ( const auto& row : pattern )
  {
    int xp=0;
    for ( char c : row ) {
      const unsigned xc = ( x + xp + X_GRID ) % X_GRID;
      const unsigned yc = ( y + yp + Y_GRID ) % Y_GRID;
      grid[ LifeCoord( xc, yc ) ].value = (c == 'X') ? 1 : 0;   
      xp += ( rotate & 1 ) ? 1 : -1;
    }
    yp += ( rotate & 2 ) ? 1 : -1;
  }
} 

// Creates the screen and initial board.  Updates the game.
class LifeSingleton
{
  public:

  LifeSingleton( const LifeSingleton& other ) = delete;
  LifeSingleton& operator=( const LifeSingleton& other ) = delete;

  LifeSingleton() 
  {
    SDL_Init(SDL_INIT_VIDEO );
    screen = SDL_SetVideoMode(X_SCREEN, Y_SCREEN, 32, SDL_SWSURFACE);

    // Draw some glider guns
    for ( int i = 0; i < 10; ++i )
    {
      dropPattern( life.first, rand() % X_GRID, rand() % Y_GRID, gliderGun, rand() % 4 ); 
    }
  }
  ~LifeSingleton()
  {
    screen = nullptr;
    SDL_Quit();
  }
  
  void update( void )
  {
    advanceSim( life );
    advanceAge( age, life.first );
    if (SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
    drawScreen( screen, life.first, age );
    if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
    SDL_UpdateRect(screen, 0, 0, 0, 0); 
  }

  private:
  LifeDBuffer life;
  LifeBuffer age;
  SDL_Surface *screen;
};

std::unique_ptr< LifeSingleton > singleton; 

// Advance forward one.  Callback from emscripten
void tick() {
  singleton->update(); 
}

int main(int argc, char** argv) 
{
  srand(time( nullptr ));
  
  singleton = std::unique_ptr< LifeSingleton >( new LifeSingleton());
  emscripten_set_main_loop(tick, 15000, 0);

  return 0;
}

