/**
 * @file   storage_manager.h
 * @author Stavros Papadopoulos <stavrosp@csail.mit.edu>
 *
 * @section LICENSE
 *
 * The MIT License
 *
 * @copyright Copyright (c) 2014 Stavros Papadopoulos <stavrosp@csail.mit.edu>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 * @section DESCRIPTION
 *
 * This file defines class StorageManager.
 */  

#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include "tile.h"
#include "array_schema.h"
#include <vector>
#include <string>
#include <map>
#include <limits>
#include <set>

/** Name of the file storing the array schema. */
#define SM_ARRAY_SCHEMA_FILENAME "array_schema"
/** Name of the file storing the fragment book-keeping info. */
#define SM_FRAGMENTS_FILENAME "fragments"
/** Name of the file storing the bounding coordinates of each tile. */
#define SM_BOUNDING_COORDINATES_FILENAME "bounding_coordinates"
/** Suffix of all book-keeping files. */
#define SM_BOOK_KEEPING_FILE_SUFFIX ".bkp"
/** Name of the file storing the MBR of each tile. */
#define SM_MBRS_FILENAME "mbrs"
/** Name of the file storing the offset of each tile in its data file. */
#define SM_OFFSETS_FILENAME "offsets"
/** 
 * The segment size determines the minimum amount of data that can be exchanged
 * between the hard disk and the main memory in a single I/O operation. 
 * Unless otherwise defined, this default size is used. 
 */
#define SM_SEGMENT_SIZE 10000000
/** Name of the file storing the id of each tile. */
#define SM_TILE_IDS_FILENAME "tile_ids"
/** Suffix of all tile data files. */
#define SM_TILE_DATA_FILE_SUFFIX ".tdt"
/** Special value returned by StorageManager::tile_pos.  */
#define SM_INVALID_POS -1
/** Special value used in FragmentInfo::lastly_appended_tile_ids_.  */
#define SM_INVALID_TILE_ID -1

/** 
 * A storage manager object is responsible for storing/fetching tiles
 * to/from the disk, and managing the tiles in main memory. It
 * maintains all the book-keeping structures and data files for the 
 * created arrays. 
 *
 * If there are m attributes in an array, a logical tile in the 
 * multi-dimensional space corresponds to m+1 physical tiles on the disk;
 * one for each of the m attributes, and one for the coordinates which
 * is regarded as an extra (m+1)-th attribute.
 * The storage manager stores the physical tiles of each attribute into
 * a separate file on the disk.
 */
class StorageManager {
 public:
  // TYPE DEFINITIONS
  struct FragmentInfo;
  /** 
   * An array or fragment is opened either to be created (CREATE mode) or to be
   * read (READ mode), but not both. 
   */
  enum Mode {READ, CREATE};

  /** Mnemonic: (first_bound_coord, last_bound_coord) */
  typedef std::pair<void*, void*> BoundingCoordinatesPair;
  /** Mnemonic: <bound_coord_pair#1, bound_coord_pair#2, ...> */
  typedef std::vector<BoundingCoordinatesPair> BoundingCoordinates;
  /** 
   * A hyper-rectangle in the logical space, including all the coordinates
   * of a tile. It is a list of lower/upper values across each dimension, i.e.,
   * (dim#1_lower, dim#1_upper, dim#2_lower, dim#2_upper, ...).
   */
  typedef void* MBR; 
  /** Mnemonic: <MBR#1, MBR#2, ...> */
  typedef std::vector<MBR> MBRs;
  /** Mnemonic: <offset#1, offset#2, ...> */
  typedef std::vector<int64_t> OffsetList;
  /** Mnemonic: [attribute_id] --> <offset#1, offset#2, ...> */
  typedef std::vector<OffsetList> Offsets;
  /** Mnemonic: [array_name + "_" + fragment_name] --> FragmentInfo */
  typedef std::map<std::string, FragmentInfo> OpenFragments;
  /** Mnemonic: [attribute_id] --> payload_size */
  typedef std::vector<size_t> PayloadSizes;
  /** Mnemonic: (pos_lower, pos_upper) */
  typedef std::pair<int64_t, int64_t> PosRange;
  /** Mnemonic: [attribute_id] --> (pos_lower, pos_upper) */
  typedef std::vector<PosRange> PosRanges;
  /** Mnemonic: [attribute_id] --> segment */
  typedef std::vector<void*> Segments;
  /** Mnemonic: <tile_id#1, tile_id#2, ...> */
  typedef std::vector<int64_t> TileIds;
  /** Mnemonic: <tile#1, tile#2, ...> */
  typedef std::vector<const Tile*> TileList;
  /** Mnemonic: [attribute_id] --> <tile#1, tile#2, ...> */
  typedef std::vector<TileList> Tiles;

  /**  Encapsulating coordinates and attributes. */
  struct CoordsAttrs {
    /** Buffer of attributtes. */ 
    const void* attrs_; 
    /** Buffer of coordinates. */ 
    const void* coords_;
    /** Number of dimensions (i.e., number of coordinates). */
    int dim_num_;
  };

  /**  Encapsulating coordinates, attributes and an id. */
  struct IdCoordsAttrs {
    /** Buffer of attributtes. */ 
    const void* attrs_; 
    /** Buffer of coordinates. */ 
    const void* coords_;
    /** Number of dimensions (i.e., number of coordinates). */
    int dim_num_;
    /** An id. */
    int64_t id_;
  };

  /**  Encapsulating coordinates, attributes and two ids. */
  struct IdIdCoordsAttrs {
    /** Buffer of attributtes. */ 
    const void* attrs_; 
    /** Buffer of coordinates. */ 
    const void* coords_;
    /** Number of dimensions (i.e., number of coordinates). */
    int dim_num_;
    /** An id. */
    int64_t id_1_;
    /** An id. */
    int64_t id_2_;
  };

  /** 
   * This struct groups info about an array fragment (e.g., schema, book-keeping
   * structures, etc.).
   */  
  struct FragmentInfo {
    /** The array schema (see class ArraySchema). */
    const ArraySchema* array_schema_;
    /** 
     * Stores the bounding coordinates of every (coordinate) tile, i.e., the 
     * first and last cell of the tile (see Tile::bounding_coordinates).
     */
    BoundingCoordinates bounding_coordinates_;   
    /** The fragment mode (see StorageManager::Mode). */
    Mode fragment_mode_;
    /** The fragment name. */
    std::string fragment_name_; 
    /** 
     * Unique FragmentInfo object id, for debugging purposes when using 
     * StorageManager::FragmentDescriptor objects 
     * (see StorageManager::FragmentDescriptor::fragment_info_id_).
     */
    int64_t id_;
    /** 
     * It keeps the id of the lastly appended tile for each attribute. It is
     * used for debugging purposes to ensure the fragment "correctness" in 
     * StorageManager::check_on_append_tile.
     */
    std::vector<int64_t> lastly_appended_tile_ids_;
    /** Stores the MBR of every (coordinate) tile. */
    MBRs mbrs_; 
    /** 
     * Stores the offset (i.e., starting position) of every tile of every 
     * attribute in the respective data file. 
     */
    Offsets offsets_;
    /** 
     * Stores the aggregate payload size of the tiles currently stored in main 
     * memory for each attribute. 
     */
    PayloadSizes payload_sizes_;
    /** 
     * Stores the range of the position of the tiles currently in main memory,
     * for each attribute. The position of a tile is a sequence number 
     * indicating the order in which it was appended to the fragment with 
     * respect to the the other tiles appended to the fragment for the same
     * attribute (e.g., 0 means that it was appended first, 1 second, etc.).
     */
    PosRanges pos_ranges_;
    /** Stores one segment per attribute. */
    Segments segments_;
    /** Stores all the tile ids of the fragment. */
    TileIds tile_ids_;
    /** Stores the tiles of every attribute currently in main memory. */
    Tiles tiles_;
  };

  /** 
   * This class is a wrapper for a FragmentInfo object. It is 
   * returned by StorageManager::open_fragment, and used to append/get tiles
   * to/from a fragment. Its purpose is to eliminate the cost of finding the
   * fragment info in the book-keeping structures (and specifically in 
   * StorageManager::open_fragments_) every time an operation must be
   * executed for this fragment (e.g., StorageManager::get_tile). It contains
   * a pointer to an FragmentInfo object in StorageManager::open_fragments_,
   * along with FragmentDescriptor::fragment_info_id_ that is used for debugging
   * purposes to check if the stored FragmentInfo object is obsolete (i.e., if 
   * it has been deleted by the storage manager when closing the fragment).
   *
   * A FragmentDescriptor object also maintains some 'write state' when
   * appending unordered logical cells to a fragment. This is necessary
   * to accommodate successive writes, and facilitate an incremental
   * sorting processes of the cells.
   */
  class FragmentDescriptor {
    // StorageManager objects can manipulate the private members of this class.
    friend class StorageManager;

   public:
    // CONSTRUCTORS & DESTRUCTORS
    /** Simple constructor. */
    explicit FragmentDescriptor(FragmentInfo* fragment_info) { 
      fragment_info_ = fragment_info;
      fragment_info_id_ = fragment_info->id_;
      array_name_ = fragment_info->array_schema_->array_name();
      fragment_name_ = fragment_info->fragment_name_;
    }
    /** Empty destructor. */
    ~FragmentDescriptor() {}

    // ACCESSORS
    /** Returns the array name. */
    const std::string& array_name() const 
        { return array_name_; } 
    /** Returns the array schema. */ 
    const ArraySchema* array_schema() const 
        { return fragment_info_->array_schema_; } 
    /** Returns the array info. */
    const FragmentInfo* fragment_info() const { return fragment_info_; } 
    /** Returns the fragment name. */
    const std::string& fragment_name() const 
        { return fragment_name_; } 

    // MUTATORS
    /** Adds a buffer that must be freed later (after it is consumed). */
    void add_buffer_to_be_freed(const void* buffer) 
        { buffers_to_be_freed_.push_back(buffer); }
 
   private:
    // PRIVATE ATTRIBUTES
    /** The array name. */
    std::string array_name_;
    /** The fragment info. */
    FragmentInfo* fragment_info_;
    /** 
     * The id of the FragmentDescriptor::fragment_info_ object. This is used for
     * debugging purposes to check if the stored FragmentInfo object is obsolete
     * (i.e., if it has been deleted by the storage manager from 
     * StorageManager::open_fragments_ when closing the fragment).
     */
    int64_t fragment_info_id_;
    /** The fragment name. */
    std::string fragment_name_;
    /** 
     * Part of the write state, which stores the pointers to the buffers to be
     * freed. 
     */
    std::vector<const void*> buffers_to_be_freed_;
    /** 
     * Part of the write state, which stores logical cells.
     * '0' stands for 'no id', i.e., the sorting will only be based on the 
     * coordinates. 
     */
    std::vector<CoordsAttrs> ws_cells_0_;
    /** 
     * Part of the write state, which stores logical cells.
     * '1' stands for '1 id', i.e., the sorting will be based on the id first, 
     * and then on the coordinates.
     */
    std::vector<IdCoordsAttrs> ws_cells_1_;
    /** 
     * Part of the write state, which stores logical cells.
     * '2' stands for '2 ids', i.e., the sorting will be based first on id_1,
     * then on id_2 and then on the coordinates.
     */
    std::vector<IdIdCoordsAttrs> ws_cells_2_;

    // PRIVATE METHODS
    /** 
     * Override the delete operator so that only a StorageManager object can
     * delete dynamically created FragmentDescriptor objects.
     */
    void operator delete(void* ptr) { ::operator delete(ptr); }
    /** 
     * Override the delete[] operator so that only a StorageManager object can
     * delete dynamically created FragmentDescriptor objects.
     */
    void operator delete[](void* ptr) { ::operator delete[](ptr); }
  };
  /** 
   * This object holds a vector of FragmentDescriptor objects and the array
   * schema. It essentially includes all the information necessary to process
   * an array.
   */
  class ArrayDescriptor {
    // StorageManager objects can manipulate the private members of this class.
    friend class StorageManager;

   public:
    // CONSTRUCTORS & DESTRUCTORS
    /** Simple constructor. */
    ArrayDescriptor(const ArraySchema* array_schema, 
                    const std::vector<const FragmentDescriptor*>& fd) { 
      fd_ = fd;
      array_schema_ = array_schema;
    }
    /** Empty destructor. */
    ~ArrayDescriptor() { 
      if(array_schema_ != NULL) 
        delete array_schema_; 
    }

    // ACCESSORS
    /** Returns the array name. */
    const std::string& array_name() const 
        { return array_schema_->array_name(); } 
    /** Returns the array schema. */
    const ArraySchema* array_schema() const 
        { return array_schema_; } 
    /** For easy access of the fragment descriptors. */
    const std::vector<const FragmentDescriptor*>& fd() const 
        { return fd_; }
    bool is_empty() const { return !fd_.size(); }
 
   private:
    // PRIVATE ATTRIBUTES
    /** The array schema. */
    const ArraySchema* array_schema_;
    /** The fragment descriptors. */
    std::vector<const FragmentDescriptor*> fd_;
    
    // PRIVATE METHODS
    /** 
     * Override the delete operator so that only a StorageManager object can
     * delete dynamically created ArrayDescriptor objects.
     */
    void operator delete(void* ptr) { ::operator delete(ptr); }
    /** 
     * Override the delete[] operator so that only a StorageManager object can
     * delete dynamically created ArrayDescriptor objects.
     */
    void operator delete[](void* ptr) { ::operator delete[](ptr); }
  };
 
  // CONSTRUCTORS & DESTRUCTORS
  /**
   * Upon its creation, a storage manager object needs a workspace path. The 
   * latter is a folder in the disk where the storage manager creates all the 
   * array data (i.e., tile and index files). Note that the input path must 
   * exist. If the workspace folder exists, the function does nothing, 
   * otherwise it creates it. The segment size determines the amount of data 
   * exchanged in an I/O operation between the disk and the main memory. 
   */
  StorageManager(const std::string& path, 
                 size_t segment_size = SM_SEGMENT_SIZE);
  /** When a storage manager object is deleted, it closes all open arrays. */
  ~StorageManager();
 
  // MUTATORS
  /** Changes the default segment size. */
  void set_segment_size(size_t segment_size) { segment_size_ = segment_size; }
   
  // ARRAY FUNCTIONS
  /** Returns true if the array has been defined. */
  bool array_defined(const std::string& array_name) const;
  /** Returns true if the array has been loaded. */
  bool array_loaded(const std::string& array_name) const;
  /** Deletes all the fragments of an array. */
  void clear_array(const std::string& array_name);
  /** Closes an array. */
  void close_array(const ArrayDescriptor* ad);
  /** 
   * Closes a fragment. In case the fragment was opened in FRAGMENT_CREATE mode,
   * a rule must be satisfied before closing the fragment: 
   * Across all attributes, the lastly appended tile must have the same id.
   */
  void close_fragment(const FragmentDescriptor* fd);
  /** Defines an array (stores its array schema). */
  void define_array(const ArraySchema* array_schema) const;
  /** It deletes an array (regardless of whether it is open or not). */
  void delete_array(const std::string& array_name);
  /** It deletes a fragment (regardless of whether it is open or not). */
  void delete_fragment(const std::string& array_name,
                       const std::string& fragment_name);
  /** 
   * Loads the contents of the file that stores the book-keeping info for 
   * the fragments of na array into the input buffer, and updates the input
   * buffer_size with the size of this file. Note that the buffer must be later
   * deleted by the caller.
   */
  void flush_fragments_bkp(const std::string& array_name, 
                           const char* buffer, size_t buffer_size) const;

  /** Returns true if the fragment is empty. */
  bool fragment_empty(const FragmentDescriptor* fd) const;
  /** Returns true if the fragment exists. */
  bool fragment_exists(
      const std::string& array_name,
      const std::string& fragment_name) const;
  /** Loads the schema of an array into the second input from the disk. */
  const ArraySchema* load_array_schema(const std::string& array_name) const;
  /** 
   * Loads the contents of the file that stores the book-keeping info for 
   * the fragments of na array into the input buffer, and updates the input
   * buffer_size with the size of this file. Note that the buffer must be later
   * deleted by the caller.
   */
  void load_fragments_bkp(const std::string& array_name, 
                          char*& buffer, size_t& buffer_size) const;
  /** Stores a new schema for an array on the disk. */
  void modify_array_schema(const ArraySchema* array_schema) const;
  /** 
   * Modifies the fragment book-keeping structures for the case of irregular 
   * tiles, when the capacity changes as part of the 'retile' query. 
   */
  void modify_fragment_bkp(const FragmentDescriptor* fd, 
                           int64_t capacity) const;
  /** 
   * Returns the begin iterator to the StorageManager::MBRList that 
   * contains the MBRs of the intput array. 
   */ 
  MBRs::const_iterator MBR_begin(const FragmentDescriptor* fd) const; 
  /** 
   * Returns the end iterator to the StorageManager::MBRList that 
   * contains the MBRs of the intput array. 
   */ 
  MBRs::const_iterator MBR_end(const FragmentDescriptor* fd) const; 
  /** Opens an array in the input mode, opening only the input fragments. */
  const ArrayDescriptor* open_array(
      const std::string& array_name,
      const std::vector<std::string>& fragment_names, Mode mode);
  /** Opens an array in the input mode, opening only the input fragments. */
  const ArrayDescriptor* open_array(
      const ArraySchema* array_schema,
      const std::vector<std::string>& fragment_names, Mode mode);
  /** Opens a fragment array in the input mode.*/
  FragmentDescriptor* open_fragment(
      const ArraySchema* array_schema, 
      const std::string& fragment_name, Mode mode);
  /** 
   * Prepares to write data of the input size into a fragment. This may 
   * trigger the sorting of the data buffered into the input fragment
   * (using external sorting), in order to appropriately free space in 
   * main memory. 
   * NOTE: This function is necessary prior to starting any sequence
   * of writes into a fragment.
   */
  void prepare_to_write(FragmentDescriptor* fd, size_t size) const;
  /** 
   * Writes a logical cell (comprised of coordinates and attributes) into
   * a fragment. 
   */
  void write_cell(FragmentDescriptor* fd, const CoordsAttrs& cell) const;
  /** 
   * Writes a logical cell (comprised of coordinates, attributes and a 
   * cell or tile id) into a fragment. 
   */
  void write_cell(FragmentDescriptor* fd, const IdCoordsAttrs& cell) const;
  /** 
   * Writes a logical cell (comprised of coordinates, attributes, a tile
   * id and a cell id) into a fragment. 
   */
  void write_cell(FragmentDescriptor* fd, const IdIdCoordsAttrs& cell) const;

  // TILE FUNCTIONS
  /**
   * Inserts a tile into the fragment. Note that tiles are always 
   * appended in the end of the corresponding attribute file.
   * Note: Two rules must be followed: (i) For each attribute,
   * tiles must be appended in a strictly ascending order of tile ids.
   * (ii) If a tile with a certain id is appended for an attribute A,
   * a tile with the same id must be appended across all attributes
   * before appending a new tile with a different tile id for A.
   */
  void append_tile(
      const Tile* tile, const FragmentDescriptor* fd, int attribute_id); 
  /**  Returns a tile of an array with the specified attribute and tile id. */
  const Tile* get_tile(
      const FragmentDescriptor* fd, int attribute_id, int64_t tile_id);  
  /**  
   * Returns a tile of an array with the specified attribute and tile position. 
   */
  const Tile* get_tile_by_pos(
      const FragmentDescriptor* fd, int attribute_id, int64_t pos);  
  /** 
   * Creates an empty tile for a specific array and attribute, with reserved 
   * capacity equal to capacity (note though that there are no constraints on
   * the number of cells the tile will actually accommodate - this is only
   * some initial reservation of memory to avoid mutliple memory expansions
   * as new cells are appended to the tile). 
   */
  Tile* new_tile(const ArraySchema* array_schema, int attribute_id, 
                 int64_t tile_id, int64_t capacity) const; 
  /** 
   * Returns the id of the tile with the input position for the input array. 
   * Note that the id of a logical tile across all attributes is the same at the
   * same position (physical tiles correspondig to the same logical tile are 
   * appended to the array in the same order).
   */
  int64_t get_tile_id(const FragmentDescriptor* ad, int64_t pos) const;

  // TILE ITERATORS
  /** This class implements a constant tile iterator. */
  class const_tile_iterator {
   public:
    /** Iterator constuctor. */
    const_tile_iterator();
    /** Iterator constuctor. */
    const_tile_iterator(
        const StorageManager* storage_manager,
        const FragmentDescriptor* fd, 
        int attribute_id,
        int64_t pos); 
    /** Assignment operator. */
    void operator=(const const_tile_iterator& rhs);
    /** Addition operator. */
    const_tile_iterator operator+(int64_t step);
    /** Addition-assignment operator. */
    void operator+=(int64_t step);
    /** Pre-increment operator. */
    const_tile_iterator operator++();
    /** Post-increment operator. */
    const_tile_iterator operator++(int junk);
    /** 
     * Returns true if the iterator is equal to that in the
     * right hand side (rhs) of the operator. 
     */
    bool operator==(const const_tile_iterator& rhs) const;
    /** 
     * Returns true if the iterator is equal to that in the
     * right hand side (rhs) of the operator. 
     */
    bool operator!=(const const_tile_iterator& rhs) const;
    /** Returns the tile pointed by the iterator. */
    const Tile* operator*() const; 
    /** Returns the array schema associated with this tile. */
    const ArraySchema* array_schema() const;
    /** Returns the bounding coordinates of the tile. */
    BoundingCoordinatesPair bounding_coordinates() const;
    /** Returns the MBR of the tile. */
    MBR mbr() const;
    /** Returns the position. */
    int64_t pos() const { return pos_; };
    /** Returns the id of the tile. */
    int64_t tile_id() const;

   private:
    /** The array descriptor corresponding to this iterator. */
    const FragmentDescriptor* fd_;
    /** The attribute id corresponding to this iterator. */
    int attribute_id_;
    /** The position of the current tile in the book-keeping structures. */
    int64_t pos_;
    /** The storage manager object that created the iterator. */ 
    const StorageManager* storage_manager_;
  };
  /** Begin tile iterator. */
  const_tile_iterator begin(const FragmentDescriptor* fd, 
                            int attribute_id) const;
  /** End tile iterator. */
  const_tile_iterator end(const FragmentDescriptor* fd,
                     int attribute_id) const;
  
  // MISC
  /** 
   * Returns the ids of the tiles whose MBR overlaps with the input range.
   * The bool variable in overlapping_tile_ids indicates whether the overlap
   * is full (i.e., if the tile MBR is completely in the range) or not.
   */
  void get_overlapping_tile_ids(
      const FragmentDescriptor* fd, 
      const void* range, 
      std::vector<std::pair<int64_t, bool> >* overlapping_tile_ids) const;
  /** 
   * Returns the ids of the tiles whose MBR overlaps with the input range.
   * The bool variable in overlapping_tile_ids indicates whether the overlap
   * is full (i.e., if the tile MBR is completely in the range) or not.
   */
  template<class T>
  void get_overlapping_tile_ids(
      const FragmentDescriptor* fd, 
      const T* range, 
      std::vector<std::pair<int64_t, bool> >* overlapping_tile_ids) const;
  /** 
   * Returns the positions of the tiles whose MBR overlaps with the input range.
   * The bool variable in overlapping_tile_pos indicates whether the overlap
   * is full (i.e., if the tile MBR is completely in the range) or not.
   */
  void get_overlapping_tile_pos(
      const FragmentDescriptor* fd, 
      const void* range, 
      std::vector<std::pair<int64_t, bool> >* overlapping_tile_pos) const;
  /** 
   * Returns the positions of the tiles whose MBR overlaps with the input range.
   * The bool variable in overlapping_tile_pos indicates whether the overlap
   * is full (i.e., if the tile MBR is completely in the range) or not.
   */
  template<class T>
  void get_overlapping_tile_pos(
      const FragmentDescriptor* fd, 
      const T* range, 
      std::vector<std::pair<int64_t, bool> >* overlapping_tile_pos) const;

  /** 
   * Writes a logical cell (consisiting of the coordinates and the rest of
   * the attributes) into a fragment. This function does not respect the
   * array global cell order, so it must properly take care of sorting
   * the cells as they come in.
   */
  void write_cell(FragmentDescriptor* fd, 
                  const void* coords, const void* attributes) const;

 private: 
  // PRIVATE ATTRIBUTES
  /** Used in FragmentInfo and FragmentDescriptor for debugging purposes. */
  static int64_t fragment_info_id_;
  /** Stores info about all currently open fragments. */
  OpenFragments open_fragments_;
   /** 
   * Determines the minimum amount of data that can be exchanged between the 
   * hard disk and the main memory in a single I/O operation. 
   */
  size_t segment_size_;
  /** 
   * Is a folder in the disk where the storage manager creates 
   * all the array data (i.e., tile and index files). 
   */
  std::string workspace_;
  
  // PRIVATE METHODS
  /** Checks on the fragment descriptor. */
  bool check_fragment_descriptor(const FragmentDescriptor* fd) const;
  /** Checks the tile MBR upon appending a tile. */
  template<class T>
  bool check_mbr_on_append_tile(const FragmentDescriptor* fd,
                                const Tile* tile) const;
  /** Checks upon appending a tile. */
  bool check_on_append_tile(const FragmentDescriptor* fd,
                            int attribute_id,
                            const Tile* tile) const;
  /** Checks upon closing a fragment. */
  bool check_on_close_fragment(const FragmentDescriptor* fd) const;
  /** Checks upon getting a tile. */
  bool check_on_get_tile(const FragmentDescriptor* fd,
                         int attribute_id,
                         int64_t tile_id) const;
  /** Checks upon getting a tile by position. */
  bool check_on_get_tile_by_pos(const FragmentDescriptor* fd,
                                int attribute_id,
                                int64_t pos) const;
  /** Checks upon opening an array. */
  bool check_on_open_fragment(const std::string& array_name, 
                              const std::string& fragment_name,
                              Mode fragment_mode) const;
  /** Creates the array folder in the workspace. */
  void create_array_directory(const std::string& array_name) const;
  /** Creates the fragment folder in the workspace. */
  void create_fragment_directory(const std::string& array_name,
                                 const std::string& fragment_name) const;
  /** Creates the workspace folder. */
  void create_workspace();
  /** Deletes the directory of the fragment, along with all its files. */
  void delete_fragment_directory(
      const std::string& array_name, const std::string& fragment_name) const;
  /** Deletes all the main-memory tiles of the fragment. */
  void delete_tiles(FragmentInfo& fragment_info) const;
  /** Deletes the main-memory tiles of a specific attribute of the fragment. */
  void delete_tiles(FragmentInfo& fragment_info, int attribute_id) const;
  /** Writes the fragment info on the disk. */
  void flush_fragment_info(FragmentInfo& fragment_info) const;
  /** Writes the bounding coordinates of each tile on the disk. */
  void flush_bounding_coordinates(const FragmentInfo& fragment_info) const;
  /** Writes the MBR of each tile on the disk. */
  void flush_MBRs(const FragmentInfo& fragment_info) const;
  /** Writes the tile offsets on the disk. */
  void flush_offsets(const FragmentInfo& fragment_info) const;
  /** Writes the tile ids on the disk. */
  void flush_tile_ids(const FragmentInfo& fragment_info) const;
  /** Writes the main-memory tiles of the fragment on the disk. */
  void flush_tiles(FragmentInfo& fragment_info) const;
  /** 
   * Writes the main-memory tiles of a specific attribute of the fragment
   * on the disk. 
   */
  void flush_tiles(FragmentInfo& fragment_info, int attribute_id) const;
  /**
   * Gets a tile of an attribute of an array using the tile position. 
   * The position of a tile is a sequence number indicating
   * the order in which it was appended to the array with respect to the
   * the other tiles appended to the array for the same attribute (e.g.,
   * 0 means that it was appended first, 1 second, etc.).
   */
  const Tile* get_tile_by_pos(FragmentInfo& fragment_info, 
                              int attribute_id, 
                              int64_t pos) const;
  /** 
   * Initializes the array info using the input mode, schema, 
   * and fragment name. 
   */
  void init_fragment_info(const std::string& fragment_name, 
                          const ArraySchema* array_schema,
                          Mode fragment_mode,
                          FragmentInfo& fragment_info) const; 
  /** Loads the array info into main memory from the disk. */
  void load_fragment_info(const std::string& array_name);
  /** Loads the bounding coordinates into main memory from the disk. */
  void load_bounding_coordinates(FragmentInfo& fragment_info) const;
  /** Loads the MBRs into main memory from the disk. */
  void load_MBRs(FragmentInfo& fragment_info) const;
  /** Loads the offsets into main memory from the disk. */
  void load_offsets(FragmentInfo& fragment_info) const;
  /** 
   * Fetches tiles from the disk into main memory. Specifically, it loads their
   * payloads into a buffer. The aggregate payload size of the tiles is equal
   * to the smallest number that exceeds StorageManager::segment_size_.
   * NOTE: Care must be taken after the call of this function to delete
   * the created buffer.
   * \param fragment_info The fragment info.
   * \param attribute_id The attribute id.
   * \param start_pos The position in the index of the id of the tile 
   * the loading will start from.
   * \param buffer The buffer that will store the payloads of the loaded tiles.
   * \return A pair (buffer_size, tiles_in_buffer), where buffer_size is the
   * size of the created buffer, and tiles_in_buffer is the number of tiles 
   * loaded in the buffer.
   */
  std::pair<size_t, int64_t> load_payloads_into_buffer(
      const FragmentInfo& fragment_info, int attribute_id,
      int64_t start_pos, char*& buffer) const;
  /** Loads the tile ids into main memory from the disk. */
  void load_tile_ids(FragmentInfo& fragment_info) const;
  /** 
   * Creates tiles_in_buffer tiles for an attribute of an array from the 
   * payloads stored in buffer.
   */
  void load_tiles_from_buffer(
      FragmentInfo& fragment_info, 
      int attribute_id,
      int64_t start_pos, char* buffer, size_t buffer_size,
      int64_t tiles_in_buffer) const;
  /** 
   * Loads tiles from the disk for a specific attribute of an array.
   * The loading starts from start_pos (recall that the tiles
   * are stored on disk in increasing id order). The number of tiles
   * to be loaded is determined by StorageManager::segment_size_ (namely,
   * the function loads the minimum number of tiles whose aggregate
   * payload exceeds StorageManager::segment_size_).
   */
  void load_tiles_from_disk(
      FragmentInfo& fragment_info, int attribute_id, int64_t start_pos) const;
  /** Returns true if the input path is an existing directory. */
  bool path_exists(const std::string& path) const;
  /** 
   * Copies the payloads of the tiles of the input fragment and attribute
   * currently in main memory into the segment buffer.
   * The file_offset is the offset in the file where the segment buffer
   * will eventually be written to.
   */
  void prepare_segment(
      FragmentInfo& fragment_info, int attribute_id, 
      int64_t file_offset, size_t segment_size, char* segment) const;
  /** Simply sets the workspace. */
  void set_workspace(const std::string& path);
  /** 
   * Returns the position of tile_id in StorageManager::FragmentInfo::tile_ids_.
   * If tile_id does not exist in the book-keeping structure, it returns
   * SM_INVALID_POS. 
   */
  int64_t tile_pos(const FragmentInfo& fragment_info, int64_t tile_id) const;
}; 

#endif