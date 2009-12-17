#ifndef _DAWG_HH
#define _DAWG_HH 1

#include <istream> // streams
#include <ostream>
#include <sstream>
#include <assert.h>

// Integer types 
#ifndef _MSC_VER
# include <stdint.h>
#else /* not _MSC_VER */
  typedef unsigned __int32 uint32_t;  // MSVC does not have stdint.h
#endif /* not _MSC_VER */

namespace DAWG {
  typedef uint32_t Index;
  typedef bool     Status;
  const bool SUCCESS = true;
  const bool FAILURE = false;

  /// Encapsulate an error message stream.
  class Error {
    public:
      inline std::ostream& operator()() {
        stream_.str("");
        return stream_;
      }
      inline std::string str() const { return stream_.str(); }
    private:
      std::ostringstream    stream_;
  };

  /// The edge of a DAWG node.
  /// Bits 0 -7 : character
  /// Bit  8    : end-of-word
  /// Bit  9    : end-of-node
  /// Bits 10-31: index of first child edge
  class Edge {
    // These are implementation details which should really be hidden, but for
    // efficiency we want Edge's methods to be inline. Since inline methods have
    // to be in the header, these have to be here too.
    static const uint32_t MASK_LETTER       = 0x000000FF; ///< Bits 0 - 7 = character
    static const uint32_t MASK_END_OF_WORD  = 0x00000100; ///< Bit      8 = end-of-word
    static const uint32_t MASK_END_OF_NODE  = 0x00000200; ///< Bit      9 = end-of-node
    static const uint32_t MASK_CHILD        = 0xFFFFFC00; ///< Bits 10-31 = index of first child edge
    static const uint32_t SHIFT_CHILD       = 10;

    public:
      /// Default constructor
      Edge() { data_ = 0; }

      /// Initialized constructor
      Edge(char _letter, bool _end_of_word = false, bool _end_of_node = false, Index _child = 0) {
        data_ = 0;
        letter(_letter);
        end_of_word(_end_of_word);
        end_of_node(_end_of_node);
        child(_child);
      }

      /// Destructor
      ~Edge() {};

      inline Edge& operator=(const Edge& other) {
        data_ = other.data_;
        return *this;
      }

      /// Comparison
      inline bool operator==(const Edge& other) const {
        return data_ == other.data_;
      }
      inline bool operator!=(const Edge& other) const {
        return data_ != other.data_;
      }

      /// The letter of this edge.
      inline char   letter()          const { return (char)(data_ & MASK_LETTER); }
      /// Whether or not a word ends at this edge.
      inline bool   end_of_word()     const { return data_ & MASK_END_OF_WORD; }
      /// Whether or not this is the last edge in a node.
      inline bool   end_of_node()     const { return data_ & MASK_END_OF_NODE; }
      /// Index of the first child edge of the node this points to.
      inline Index  child()           const { return (data_ & MASK_CHILD) >> SHIFT_CHILD; }

      /// Set the letter.
      inline void   letter(char c)          { data_ = (data_ & ~MASK_LETTER) | c; }
      /// Set the end-of-word flag.
      inline void   end_of_word(bool v)     { if (v) data_ |= MASK_END_OF_WORD; else data_ &= ~MASK_END_OF_WORD; }
      /// Set the end-of-node flag.
      inline void   end_of_node(bool v)     { if (v) data_ |= MASK_END_OF_NODE; else data_ &= ~MASK_END_OF_NODE; }
      /// Set the child index.
      inline void   child(Index n)          { data_ = (data_ & ~MASK_CHILD) | (n << SHIFT_CHILD); }

      inline uint32_t data()               const { return data_; }
      inline void print(std::ostream& out) const { out << "(" << letter() << " -> " << child() << " eow:" << end_of_word() << " eon:" << end_of_node() << ")"; }

    private:
      uint32_t data_;
  };

  /// A Directed Acyclic Word Graph.
  class DAWG {
    public:
      /// Default constructor
      DAWG() : num_edges_(0), edges_(NULL) {};

      /// Destructor
      ~DAWG();

      /// Clear DAWG.
      void clear();

      /// Load DAWG data from a stream.
      Status load(
          std::istream& input   ///< Stream containing DAWG data.
      );

      /// Load DAWG from binary data. The data will be copied.
      Status load(
          Index         num_edges,  ///< Number of edges in the data
          const Edge*   edges       ///< The actual edge data
      );

      /// Save DAWG data to a stream.
      Status save(
          std::ostream& output  ///< Steam to write DAWG data to.
      );

      /// Find an edge in a node with the specified letter.
      /// @return   an iterator pointing to the edge if found, or end() if not
      class Iterator find_edge(
          char letter,                      ///< The letter to look for
          const class Iterator& start       ///< Where to start looking
      ) const;

      /// See if a word is in the DAWG.
      bool contains_word(
          const std::string& word   ///< Word to look for
      );

      /// Get a pointer to an individual edge.
      inline Edge* edge(
          Index index           ///< index of the edge to retrieve
      ) const {
        return &edges_[index];
      }

      class Iterator root()  const; ///< Iterator pointing before the first edge
      class Iterator begin() const; ///< Iterator pointing to the first edge
      class Iterator end()   const; ///< Iterator pointing to the null edge

      /// Last error message.
      inline const std::string error() const { return error_.str(); }

    private:
      Index                 num_edges_;     ///< Number of edges in the dawg
      Edge*                 edges_;         ///< Edges
      Error                 error_;
  };

  /// A class to create a DAWG.
  class Creator {
    public:
      /// Default constructor
      Creator();

      /// Destructor
      ~Creator();

      /// Initialize internal structures for creating a DAWG.
      Status start();

      /// Add a word to the DAWG. Words must be fed in alphabetic order.
      Status add_word(
          std::string word  ///< The word to add.
      );

      /// Create final DAWG and clean up internal structures.
      /// @return   a new DAWG on success, NULL on failure
      DAWG* finish();

      /// Last error message.
      inline const std::string error() const { return error_.str(); }

    private:
      Index         num_edges_;     ///< Current number of edges
      Edge*         edges_;         ///< Edge data
      Index*        hash_table_;    ///< Hash table to speed finding of edges
      Edge*         edge_stack_;
      Index*        num_edges_stack_;
      Index         stack_pos_;
      Error         error_;

      /// Clear data
      void          clear();
      Edge*         get_edge( Index stack_pos, Index edge );
      Edge*         get_cur_edge( Index stack_pos );
      Status        finish_node( Index stack_pos );
      Status        find_hash_index( Edge* edges, Index num_edges, Index* out_hash_index );
      Index         compute_hash( Edge* edges, Index num_edges );
  };

  /// An iterator to walk through a DAWG.
  class Iterator {
    public:
      /// Empty constructor
      Iterator() : dawg_(NULL), index_(0), data_(NULL) {}

      /// Basic constructor
      Iterator(
          const DAWG*   dawg,       ///< Parent DAWG
          Index         index       ///< Index of the node to point to
      ) : dawg_(dawg), index_(index), data_(dawg_->edge(index_)) {}

      /// Copy constructor
      Iterator(
          const Iterator& other       ///< Iterator to copy.
       ) : dawg_(other.dawg_), index_(other.index_) , data_(other.data_) {}

      ~Iterator() {};

      /// Dereference
      inline Edge& operator*() {
        assert( data_ != NULL );
        return *data_;
      }
      const inline Edge& operator*() const {
        assert( data_ != NULL );
        return *data_;
      }
      inline Edge* operator->() const {
        assert( data_ != NULL );
        return data_;
      }

      /// Prefix increment. For convenience, incrementing past the end of a node
      /// goes to the NULL edge.
      inline Iterator& operator++() {
        if ( data_->end_of_node() ) {
          index_ = 0;
          data_  = dawg_->edge(0);
        } else {
          ++index_;
          ++data_;
        }
        assert( data_ = dawg_->edge(index_) );
        return *this;
      }

      /// Prefix decrement. 
      inline Iterator& operator--() {
        --index_;
        --data_;
        assert(data_ == dawg_->edge(index_));
        assert(data_ >= dawg_->edge(0));
        return *this;
      }

      Iterator  child() {
        assert( dawg_ != NULL );
        assert( data_ != NULL );
        return Iterator( dawg_, data_->child() );
      }
      Iterator  end()   {
        assert( dawg_ != NULL );
        return dawg_->end();
      }

      Iterator find_edge(char letter) const {
        assert( dawg_ != NULL );
        return dawg_->find_edge( letter, *this );
      }

      /// Comparison
      inline bool operator==(const Iterator& other) const {
        return data_ == other.data_;
      }
      inline bool operator!=(const Iterator& other) const {
        return data_ != other.data_;
      }

    private:
      const DAWG*   dawg_;
      Index         index_;
      Edge*         data_;

  };
}

#endif /* not _DAWG_HH */
