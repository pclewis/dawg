#include "dawg.hh"
#include <iostream>

namespace DAWG {

  const uint32_t HASH_TABLE_SIZE    = 1000003;              /// Size of hash table - use a prime number.
  const uint32_t MAX_EDGES          = (HASH_TABLE_SIZE-1);  /// Maximum number of edges that can be created.
  const uint32_t MAX_CHARS          = 256;                  /// Maximum number of characters in a node.
  const uint32_t MAX_WORD_LENGTH    = 32;                   /// Maximum length of a word.
  typedef uint32_t Magic;                                   /// Special type for magic number.
  const Magic    MAGIC_NUMBER       = 0xC6ACC231;           /// Arbitrary number to identify files we write.

  //----------------------------------------------------------------------------//
  // DAWG                                                                       //
  //----------------------------------------------------------------------------//

  // Destructor
  DAWG::~DAWG() {
    clear();
  }

  // Clear DAWG
  void DAWG::clear() {
    // Free nodes if needed
    if (edges_ != NULL)
      delete [] edges_;
    edges_ = NULL;
    // Update count
    num_edges_ = 0;
  }

  // Load DAWG data from a stream
  Status DAWG::load( std::istream& input ) {
    assert( input.good() );
    clear(); // clear any old data

    std::streamsize     num_read    = 0;
    Magic               magic       = 0;
    Index               num_edges   = 0;

    // read magic number
    input.read( (char*)&magic, sizeof(magic) );
    num_read = input.gcount();
    if ( num_read != sizeof(magic) ) {
      error_() << "Couldn't read file identifier: Expected " << sizeof(magic)
               << " bytes but got " << num_read << ".";
      // failure
      return FAILURE;
    }

    // check magic number
    if ( magic != MAGIC_NUMBER ) {
      error_() << "File identifier mismtached: Expected " << MAGIC_NUMBER
               << " but got " << magic;
      return FAILURE;
    }

    // read number of edges
    input.read( (char*)&num_edges, sizeof(num_edges) );
    num_read = input.gcount();
    if ( num_read != sizeof(num_edges) ) {
      error_() << "Couldn't read number of edges: Expected " << sizeof(num_edges)
               << " bytes but got " << num_read << ".";
      // failure
      return FAILURE;
    }

    // allocate space for edges
    edges_ = new Edge[num_edges + 1];

    // read in data
    input.read( (char*)edges_, sizeof(Edge) * num_edges );
    num_read = input.gcount(); 
    if ( (unsigned)num_read != (sizeof(Edge) * num_edges) ) {
      error_() << "Couldn't read edges: Expected " << (sizeof(Edge) * num_edges)
               << " bytes but got " << num_read << ".";

      // data is no good, clear it
      clear();

      // failure
      return FAILURE;
    }

    // set edge count
    num_edges_ = num_edges;

    // init root edge
    edge(num_edges_)->child(1);

    // success
    return SUCCESS;

  }

  // Load DAWG from binary data.
  Status DAWG::load( Index num_edges, const Edge* edges ) {
    // clear any old data
    clear();
    // allocate space for edges
    edges_ = new Edge[num_edges+1];
    // copy edges
    memcpy( (void*) edges_, (void*) edges, sizeof(Edge) * num_edges );
    // update edge count
    num_edges_ = num_edges;
    // init root edge
    edge(num_edges_)->child(1);
    // success
    return SUCCESS;
  }

  // Save DAWG to stream.
  Status DAWG::save( std::ostream& out ) {
    // make sure the stream is good
    assert( out.good() );

    // write magic number
    out.write( (const char*) &MAGIC_NUMBER, sizeof(MAGIC_NUMBER) );
    if ( out.fail() ) {
      error_() << "Couldn't write magic number";
      return FAILURE;
    }

    // write number of nodes
    out.write( (const char*) &num_edges_, sizeof(num_edges_) );
    if ( out.fail() ) {
      error_() << "Couldn't write number of edges";
      return FAILURE;
    }

    // write node data
    out.write( (const char*) edges_, sizeof(Edge) * num_edges_ );
    if ( out.fail() ) {
      error_() << "Couldn't write data";
      return FAILURE;
    }

    // success
    return SUCCESS;
  }

  Iterator DAWG::find_edge( char letter, const Iterator& start ) const {
    Iterator i = start;
    while ( i != end() ) {
      if ( i->letter() == letter )
        break;
      else
        ++i;
    }

    return i;
  }

  bool DAWG::contains_word(const std::string& word) {
    std::string::const_iterator si;
    Iterator                    di = begin();
    bool                        eow = false;

    for ( si = word.begin(); si != word.end(); ++si ) {
      di  = find_edge(*si, di);
      if ( di == end() ) {
        return false;
      }
      eow = di->end_of_word();
      di  = di.child();
    }

    return eow;
  }


  // Iterator pointing before first edge
  Iterator DAWG::root() const { return Iterator( this, num_edges_ ); }
  // Iterator pointing to first edge
  Iterator DAWG::begin() const { return Iterator( this, 1 ); }
  // Iterator pointing to null edge
  Iterator DAWG::end()   const { return Iterator( this, 0 ); }

  //----------------------------------------------------------------------------//
  // DAWG Creator                                                               //
  //----------------------------------------------------------------------------//

  Creator::Creator() {
    edges_          = NULL;
    num_edges_      = 0;
    hash_table_     = NULL;
    edge_stack_     = NULL;
    num_edges_stack_= NULL;
    stack_pos_      = 0;
  }

  Creator::~Creator() {
    clear();
  }

  void Creator::clear() {
    if ( edges_ != NULL )
      delete [] edges_;
    edges_      = NULL;
    num_edges_  = 0;

    if ( hash_table_ != NULL )
      delete [] hash_table_;
    hash_table_ = NULL;

    if ( edge_stack_ != NULL )
      delete [] edge_stack_;
    edge_stack_ = NULL;

    if ( num_edges_stack_ != NULL )
      delete [] num_edges_stack_;
    num_edges_stack_ = NULL;

    stack_pos_  = 0;
  }

  /// Initialize internal structures for creating a DAWG.
  Status Creator::start() {
    assert( hash_table_         == NULL );
    assert( edges_              == NULL );
    assert( edge_stack_         == NULL );
    assert( num_edges_stack_    == NULL );

    edges_          = new Edge[MAX_EDGES];
    edge_stack_     = new Edge[MAX_CHARS * MAX_WORD_LENGTH];
    hash_table_     = new Index[HASH_TABLE_SIZE];
    memset( (void*)hash_table_, 0, sizeof(Index) * HASH_TABLE_SIZE );
    num_edges_stack_= new Index[MAX_WORD_LENGTH];
    memset( (void*)num_edges_stack_, 0, sizeof(Index) * MAX_WORD_LENGTH );
    stack_pos_      = 0;
    
    // The first node is reserved for the null node, and the first MAX_CHARS
    // nodes are reserved for the bottom of the tree.
    num_edges_      = 1 + MAX_CHARS;

    return SUCCESS;
  }
  
  Edge* Creator::get_edge( Index stack_pos, Index edge ) {
    return edge_stack_ + (stack_pos * MAX_CHARS + edge);
  }
  Edge* Creator::get_cur_edge( Index stack_pos ) {
    return get_edge( stack_pos, num_edges_stack_[stack_pos]-1 );
  }

  /// Add a word to the DAWG.
  Status Creator::add_word( std::string word ) {
    // Check preconditions
    assert( hash_table_         != NULL );
    assert( edges_              != NULL );
    assert( edge_stack_         != NULL );
    assert( num_edges_stack_    != NULL );

    // Make sure word will fit.
    if ( word.length() >= MAX_WORD_LENGTH ) {
      error_() << "Word is too long (\"" << word << "\" is " << word.length()
               << " chars, max is " << MAX_WORD_LENGTH << ")";
      return FAILURE;
    }

    // If stack position isn't 0
    if ( stack_pos_ > 0 ) {
      // Find the first different letter in the stack
      Index i;
      for ( i = 0; i <= stack_pos_ && i < word.length(); i++ ) {
        if ( word[i] != get_cur_edge(i)->letter() )
          break;
      }

      // If there's a difference before the current stack position
      if ( i <= stack_pos_ ) {
        //std::cout << "difference! " << word << "[" << i << "](" << word[i] << ") != " << get_cur_edge(i)->letter() << std::endl;
        // Make sure word is in order
        if ( word[i] < get_cur_edge(i)->letter() ) {
          error_() << "Word out of order: " << word << "[" << i << "] (" << word[i] << " < " << get_cur_edge(i)->letter() << ")";
          return FAILURE;
        }
        // Finish all nodes above the difference
        for ( ; stack_pos_ > i; --stack_pos_ ) {
          Status status = finish_node( stack_pos_ );
          if ( status != SUCCESS ) {
            return status;
          }
        }
      }
      // If there's a difference after the current stack position
      else if ( i > stack_pos_ ) {
        // move the stack up a level, so we 
        // start adding letters after the end of the last word
        ++stack_pos_;
      }
    }

    // Add each additional letter to the stack  
    for ( ; stack_pos_ < word.length(); ++stack_pos_ ) {
      //std::cout << "adding " << word[stack_pos_] << " at " << stack_pos_ << std::endl;
      ++num_edges_stack_[stack_pos_];
      get_cur_edge(stack_pos_)->letter( word[stack_pos_] );
    }
    // stack_pos_ will be word.length() now (ie 1 past end), move it back
    --stack_pos_;

    //std::cout << "stack: ";
    //for ( int i = 0 ; i <= stack_pos_ ; ++i ) {
    //  std::cout << get_cur_edge(i)->letter();
    //}
    //std::cout << std::endl;

    // Set end of word flag
    get_cur_edge(stack_pos_)->end_of_word(true);

    // Success
    return SUCCESS;
  }

  DAWG* Creator::finish() {
    // Check preconditions
    assert( hash_table_         != NULL );
    assert( edges_              != NULL );
    assert( edge_stack_         != NULL );
    assert( num_edges_stack_    != NULL );

    // Finish all remaining nodes
    for ( ; stack_pos_ > 0; --stack_pos_ ) {
      Status status = finish_node( stack_pos_ );
      if ( status != SUCCESS )
        return NULL;
    }

    // Set end-of-node on last used edge
    get_cur_edge(0)->end_of_node(true);

    // Copy the bottom of the stack into into the beginning of the DAWG
    Index i;
    for ( i = 0; i < MAX_CHARS; ++i )
      edges_[1+i] = *get_edge( 0, i );

    // Set end-of-node on last opening edge
    edges_[1+i-1].end_of_node(true);

    // Create the DAWG
    DAWG* new_dawg = new DAWG;
    new_dawg->load( num_edges_, edges_ );

    // Clear our data
    clear();

    // Return the DAWG
    return new_dawg;
  }

  Status Creator::finish_node(Index pos) {
    //std::cout << "finish_node(" << pos << ")" << std::endl;
    // Set end-of-node on last node
    get_cur_edge(pos)->end_of_node(true);

    // Find our spot in the hash table
    Index hash_idx;
    Status status = find_hash_index( get_edge(pos, 0), num_edges_stack_[pos], &hash_idx );
    if ( status != SUCCESS ) {
      // error will be set by find_hash_index()
      return status;
    }

    // Get the index from the hash table
    Index idx       = hash_table_[hash_idx];

    // If there's no matching node
    if ( idx == 0 ) {
      // Make sure DAWG isn't full
      if ( num_edges_ + num_edges_stack_[pos] > MAX_EDGES ) {
        error_() << "DAWG is full";
        return FAILURE;
      }

      idx = num_edges_;

      // Copy edges into DAWG
      Index i;
      for ( i = 0; i < num_edges_stack_[pos]; ++i ) {
        edges_[idx + i] = *get_edge(pos, i);
      }

      // Add to hash table
      hash_table_[hash_idx] = idx;

      // Update edge count
      num_edges_ += num_edges_stack_[pos];
    }

    // Make parent edge point to us
    //std::cout << pos << "-1->child(" << idx <<" '" << get_edge(pos,0)->letter() << "')" << std::endl;
    get_cur_edge(pos - 1)->child( idx );

    // Clear this stack position
    memset( (void*) get_edge(pos, 0), 0, sizeof(Edge) * MAX_CHARS );
    num_edges_stack_[pos] = 0;

    // Success
    return SUCCESS;
  }

  Status Creator::find_hash_index( Edge* edges, Index num_edges, Index* out_hash_index ) {
    Index idx           = compute_hash( edges, num_edges ) % HASH_TABLE_SIZE;
    Index first_idx     = idx;
    Index step          = 9;

    //std::cout << "hash for '" << edges[0].letter() << "' = " << idx << std::endl;
    //std::cout << "entry is --> " << hash_table_[idx] << std::endl;
    // Loop
    for (;;) {
      // If there's no entry at this position, hand it back
      if ( hash_table_[idx] == 0 ) {
        *out_hash_index = idx;
        return SUCCESS;
      }

      // See if the node at this entry matches
      Index i;
      Index start = hash_table_[idx];
      for ( i = 0; i < num_edges; ++i )
        if ( edges_[start + i] != edges[i] ) break;

      // If so, return this index
      if ( i == num_edges ) {
        *out_hash_index = idx;
        return SUCCESS;
      }

      // Otherwise, look further in the table
      idx   += step;
      step  += 9;
      if ( idx >  HASH_TABLE_SIZE ) idx  -= HASH_TABLE_SIZE;
      if ( step > HASH_TABLE_SIZE ) step -= HASH_TABLE_SIZE;

      // If we're back where we started, return an error
      if ( idx == first_idx ) {
        error_() << "Hash table is full";
        return FAILURE;
      }
    }
    // Should never get here!
    assert(false);
    error_() << "Internal Error";
    return FAILURE;
  }

  Index Creator::compute_hash( Edge* edges, Index num_edges ) {
    Index result = 0;
    for ( Index i = 0; i < num_edges; ++i )
      result = ((result << 1) | (result >> 31)) ^ edges[i].data();
    return result;
  }

}
