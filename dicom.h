// -*- c++ -*-
//
// Time-stamp: <2018-11-01 09:18:40 zophos>
//
///
/// @file   dicom.h
/// @author NISHI, Takao <zophos@ni.aist.go.jp>
/// @date   Sun Aug 17 13:32:48 2014
/// 
/// @brief  parse DICOM image file
/// 

#ifndef __VVV_DICOM_H__

#define __VVV_DICOM_H__

#include <stdint.h>

//
// based on Stack Overflow
// "Cross-platform definition of _byteswap_uint64 and _byteswap_ulong"
// https://stackoverflow.com/questions/41770887/cross-platform-definition-of-byteswap-uint64-and-byteswap-ulong
//
#ifdef _MSC_VER

#include <stdlib.h>
#define bswap_16(x) _byteswap_ushort(x)
#define bswap_32(x) _byteswap_ulong(x)
#define bswap_64(x) _byteswap_uint64(x)

#elif defined(__APPLE__)

// Mac OS X / Darwin features
#include <libkern/OSByteOrder.h>
#define bswap_16(x) OSSwapInt16(x)
#define bswap_32(x) OSSwapInt32(x)
#define bswap_64(x) OSSwapInt64(x)

#elif defined(__sun) || defined(sun)

#include <sys/byteorder.h>
#define bswap_16(x) BSWAP_16(x)
#define bswap_32(x) BSWAP_32(x)
#define bswap_64(x) BSWAP_64(x)

#elif defined(__FreeBSD__)

#include <sys/endian.h>
#define bswap_16(x) bswap16(x)
#define bswap_32(x) bswap32(x)
#define bswap_64(x) bswap64(x)

#elif defined(__OpenBSD__)

#include <sys/types.h>
#define bswap_16(x) swap16(x)
#define bswap_32(x) swap32(x)
#define bswap_64(x) swap64(x)

#elif defined(__NetBSD__)

#include <sys/types.h>
#include <machine/bswap.h>
#if defined(__BSWAP_RENAME) && !defined(__bswap_32)
#define bswap_16(x) bswap16(x)
#define bswap_32(x) bswap32(x)
#define bswap_64(x) bswap64(x)
#endif

#else

#include <byteswap.h>

#endif

#include <string.h>

#include <istream>
#include <map>
#include <vector>
#include <string>
#include <utility>
#include <exception>
#include <stdexcept>

#include <boost/any.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include <opencv2/core/core.hpp>

#ifdef DEBUG
#include <stdio.h>
#include <stdlib.h>
#endif

namespace VVV
{
    ///
    /// reading DICOM format image file class
    ///
    class Dicom
    {
    public:
        class ParseError
            :public std::runtime_error
        {
        public:
            explicit ParseError(const std::string &s)
                :std::runtime_error(s)
            {}
            explicit ParseError(const char *c)
                :std::runtime_error(c)
            {}
        };

        class StreamError
            :public ParseError
        {
        public:
            explicit StreamError(const std::string &s)
                :ParseError(s)
            {}
            explicit StreamError(const char *c)
                :ParseError(c)
            {}
        };

        class MissingTagError
            :public ParseError
        {
        public:
            explicit MissingTagError(const std::string &s)
                :ParseError(s)
            {}
            explicit MissingTagError(const char *c)
                :ParseError(c)
            {}
        };


        typedef union{
            uint16_t id[2];
            uint32_t number;
            char raw[4];
        } TypeTag;

        typedef union{
            uint16_t number;
            char raw[2];
        } TypeVR;


        ///
        /// reading each DICOM Element class
        ///
        class Element
        {
            friend class Dicom;

        public:
            ///
            /// defualt constractor 
            ///
            ///
            Element()
                :_parent(NULL),
                 _is_vector(false)
            {
                this->_vr.number=0;
                this->_tag.number=0;
            }
            
            ///
            /// constractor
            ///
            /// @param parent Dicom object
            ///
            Element(Dicom *parent)
            {
                this->_parent=parent;
            }
            
            ///
            /// copy constructor
            ///
            /// @param e 
            ///
            Element(const Element &e)
            {
                this->_parent=e._parent;
                this->_tag=e._tag;
                this->_vr=e._vr;
                this->_value=e._value;
                this->_is_vector=e._is_vector;
            }
            
            ///
            /// constructor with parse
            ///
            /// @param parent Dicom object
            /// @param ist input stream
            ///
            Element(Dicom *parent,std::istream &ist)
            {
                this->_parent=parent;
                this->parse(ist);
            }
            
            ///
            /// reader accessor: Element tag
            ///
            ///
            /// @return Element tag
            ///
            inline TypeTag tag(){ return this->_tag; }

            ///
            /// reader accessor: VR
            ///
            ///
            /// @return Element VR
            ///
            inline TypeVR vr(){ return this->_vr; }

            ///
            /// value is a vector or not
            ///
            ///
            /// @return value is a vector: true || false
            ///
            inline bool is_vector(){ return this->_is_vector; }

            ///
            /// value is empty or not
            ///
            ///
            /// @return true if value is empty; false
            ///
            inline bool empty(){ return this->_value.empty(); }

            ///
            /// type_info of value
            ///
            ///
            /// @return type_info of value
            ///
            inline const std::type_info &type(){ return this->_value.type(); }


            ///
            /// reader accessor: element value
            ///
            ///
            /// @return any type of element value
            ///
            inline boost::any value(){ return this->_value; }

            ///
            /// reader accessor: element value
            ///
            ///
            /// @return specify class of element value
            ///
            /// throw boost::bad_any_cast
            ///
            template <class T> T as()
            {
                return boost::any_cast<T>(this->_value);
            }

            ///
            /// stream parser
            ///
            /// @param ist input stream
            ///
            /// @return this object
            ///
            /// throw StreamError when met EOF, ParseError when met unknown VR
            ///
            Element &parse(std::istream &ist)
            {
                this->parse_tag(ist);
                this->parse_value(ist);

                return *this;
            }

            ///
            /// element tag parser
            ///
            /// @param ist input stream
            ///
            /// @return parsed element tag
            ///
            /// throw StreamError when met EOF
            ///
            TypeTag parse_tag(std::istream &ist)
            {
                ist.read(this->_tag.raw,4);
                if(ist.eof() || !ist.good())
                    throw StreamError("");

                if(this->_need_byte_swap()){
                    this->_tag.id[0]=bswap_16(this->_tag.id[0]);
                    this->_tag.id[1]=bswap_16(this->_tag.id[1]);
                }

#ifdef DEBUG
                fprintf(stderr,"0x%04x,0x%04x ",
                        this->_tag.id[0],
                        this->_tag.id[1]);
#endif

                return this->_tag;
            }

            ///
            /// rewind a stream much as element tag
            ///
            /// @param ist input stream
            ///
            /// @return rewinded input stream
            ///
            std::istream &rewind_tag(std::istream &ist)
            {
#ifdef DEBUG
                fprintf(stderr,"<<<<\n");
#endif
                return ist.seekg(-4,std::ios_base::cur);
            }
            
            ///
            /// parse element VR and value
            ///
            /// @param ist input stream
            ///
            /// @return this object
            ///
            /// This method shold be called after Element::parse_tag(). 
            ///
            Element &parse_value(std::istream &ist)
            {
                if(!this->_tag.number)
                    throw ParseError("No Tag Id found.");

                if(this->_format_as_explicit())
                    return this->_parse_value_explicit(ist);
                else
                    return this->_parse_value_implicit(ist);
                
                return *this;
            }

        private:
            Dicom *_parent;
            TypeTag _tag;
            TypeVR _vr;
            boost::any _value;
            bool _is_vector;

            Element &_set_parent(Dicom *parent)
            {
                this->_parent=parent;
                return *this;
            }

            bool _need_byte_swap()
            {
                if(this->_parent)
                    return this->_parent->_need_byte_swap();
                else
                    return false;
            }

            bool _architecture_as_little_endian()
            {
                if(this->_parent)
                    return this->_parent->_architecture_as_little_endian;
                else
                    return true;
            }

            bool _format_as_explicit()
            {
                if(this->_parent)
                    return this->_parent->_format_as_explicit;
                else
                    return true;
            }

            Element &_parse_value_implicit(std::istream &ist)
            {
#ifdef DEBUG
                fprintf(stderr,"** ");
#endif

                union{
                    uint32_t numeric;
                    char raw[4];
                } size;

                ist.read(size.raw,4);
                if(ist.eof() || !ist.good())
                    throw StreamError("");

                if(this->_need_byte_swap())
                    size.numeric=bswap_32(size.numeric);
                
#ifdef DEBUG
                fprintf(stderr,"%u\n",size.numeric);
#endif
                return this->_read_element_data_sequence(ist,size.numeric);
            }

            Element &_parse_value_explicit(std::istream &ist)
            {
                //
                // get VR
                //
                ist.read(this->_vr.raw,2);
                if(ist.eof() || !ist.good())
                    throw StreamError("");

#ifdef DEBUG
                fprintf(stderr,"%C%C ",
                        this->_vr.raw[0],
                        this->_vr.raw[1]);
#endif

                if(this->_architecture_as_little_endian())
                    this->_vr.number=bswap_16(this->_vr.number);

                //
                // get data length
                //
                size_t sz;
                switch(this->_vr.number){
                case 0x4f42:  // OB
                case 0x4f57:  // OW
                case 0x4f46:  // OF
                case 0x5351:  // SQ
                case 0x5554:  // UT
                case 0x554e:  // UN
                    ist.seekg(2,std::ios_base::cur); // skip 2byte

                    uint32_t ui32;
                    ist.read((char *)&ui32,4);
                    if(this->_need_byte_swap())
                        ui32=bswap_32(ui32);
                    
                    sz=(size_t)ui32;
                    break;
                default:
                    uint16_t ui16;
                    ist.read((char *)&ui16,2);
                    if(this->_need_byte_swap())
                        ui16=bswap_16(ui16);
                    
                    sz=(size_t)ui16;
                    break;
                }

                if(ist.eof() || !ist.good())
                        throw StreamError("");
                
#ifdef DEBUG
                fprintf(stderr,"%u\n",(uint16_t)sz);
#endif
                
                //
                // get data body
                //
                switch(this->_vr.number){
                case 0x4353:  // CS
                case 0x4441:  // DA
                case 0x4453:  // DS
                case 0x4454:  // DT
                case 0x4953:  // IS
                case 0x4c4f:  // LO
                case 0x4c54:  // LT
                case 0x504e:  // PN
                case 0x5348:  // SH
                case 0x5354:  // ST
                case 0x544d:  // TM
                case 0x5549:  // UI
                case 0x5554:  // UT
                    return this->_read_element_data_string(ist,sz);
                    break;
                case 0x4f42:  // OB
                case 0x554e:  // UN
                    return this->_read_element_data<char>(ist,sz);
                    break;
                case 0x5353:  // SS
                    return this->_read_element_data<int16_t>(ist,sz);
                    break;
                case 0x534c:  // SL
                    return this->_read_element_data<int32_t>(ist,sz);
                    break;
                case 0x5553:  // US
                case 0x4154:  // AT
                case 0x4f57:  // OW
                    return this->_read_element_data<uint16_t>(ist,sz);
                    break;
                case 0x554c:  // UL
                    return this->_read_element_data<uint32_t>(ist,sz);
                    break;
                case 0x464c:  // FL
                case 0x4f46:  // OF
                    return this->_read_element_data<float>(ist,sz);
                    break;
                case 0x4644:  // FD
                    return this->_read_element_data<double>(ist,sz);
                    break;
                case 0x5351:  // SQ
                    return this->_read_element_data_sequence(ist,sz);
                    break;
                }
                
                throw ParseError("Unknown VR found");

                return *this;
            }

            template <class T>
            T _read_element_data_single(std::istream &ist,size_t s)
            {
                T value;
                ist.read((char *)&value,s);

                if(ist.eof() || !ist.good())
                        throw StreamError("");

                int sz=(int)sizeof(T);
                if(sz>1 && this->_need_byte_swap()){
                    //
                    // bswap_64() could not handle double.
                    // So I give up to use bswap_??() at here.
                    //
                    unsigned char s[sz],d[sz];
                    memcpy(s,&value,sz);
                    for(int i=0,j=sz-1;i<sz;i++,j--)
                        d[j]=s[i];
                    memcpy(&value,d,sz);
                }
                
                return value;
            }
            
            template <class T>
            Element &_read_element_data(std::istream &ist,size_t len)
            {
                size_t s=sizeof(T);
                int n=len/s;
                
                if(n==1){
                    this->_value=this->_read_element_data_single<T>(ist,s);
                    this->_is_vector=false;
                }
                else{
                    std::vector<T> buf;
                    for(int i=0;i<n;i++)
                        buf.push_back(this->_read_element_data_single<T>(ist,s));
                    
                    this->_value=buf;
                    this->_is_vector=true;
                }

                return *this;
            }
            
            Element &_read_element_data_string(std::istream &ist,size_t len)
            {
                char buf[len];
                ist.read(buf,len);

                if(ist.eof() || !ist.good())
                        throw StreamError("");
            
                this->_value=std::string(buf,len);
                this->_is_vector=false;

                return *this;
            }

            Element &_read_element_data_sequence(std::istream &ist,size_t len)
            {
                //
                // when size was known
                //
                if(len!=0xFFFFFFFF)
                    return this->_read_element_data<unsigned char>(ist,len);

                //
                // when unknown size gaven
                //
                unsigned char eos[8]={0,0,0,0,0,0,0,0};

                std::vector<unsigned char> value;
                unsigned char c;
                while(true){
                    ist.read((char *)&c,1);
                    if(ist.eof() || !ist.good())
                        throw StreamError("");

                    value.push_back(c);
                
                    //for(int i=0;i<7;i++)
                    //    eos[i]=eos[i+1];
                    memmove(eos,eos+1,7);
                    eos[7]=c;

                    // check end of sequence 0xFF FE E0 DD 00 00 00 00
                    if(eos[0]==0xFF &&
                       eos[1]==0xFE &&
                       eos[2]==0xE0 &&
                       eos[3]==0xDD &&
                       eos[4]==0x00 &&
                       eos[5]==0x00 &&
                       eos[6]==0x00 &&
                       eos[7]==0x00)
                        break;
                }

                // erase end of sequence
                if(value.size()>=8){
                    for(int i=0;i<8;i++)
                        value.pop_back();
                }

                // erase start of sequence 0xFE FF E0 00
                if(value.size()>=4 && 
                   value[0]==0xFE &&
                   value[1]==0xFF &&
                   value[2]==0xE0 &&
                   value[3]==0x00){
                    std::vector<unsigned char>::iterator itr=value.begin();
                    value.erase(itr,itr+4);
                }

                this->_value=value;
                this->_is_vector=true;

                return *this;
            }
        };
        //
        // end of Dicom::Element
        //

    public:
        const static uint16_t TAG_GROUP_META;//=0x0002;
        const static uint16_t TAG_GROUP_DIRECTORY;//=0x0004;

        const static TypeTag TAG_TRANSFER_SYNTAX_UID;//={{0x0002,0x0010}};
        const static TypeTag TAG_IMG_POSITION;//={{0x0020,0x0032}};
        const static TypeTag TAG_PHOTO_INTERPRET;//={{0x0028,0x0004}};
        const static TypeTag TAG_ROWS;//={{0x0028,0x0010}};
        const static TypeTag TAG_COLS;//={{0x0028,0x0011}};
        const static TypeTag TAG_PX_SPACING;//={{0x0028,0x0030}};
        const static TypeTag TAG_BIT_ALLOC;//={{0x0028,0x0100}};
        const static TypeTag TAG_BIT_STORED;//={{0x0028,0x0101}};
        const static TypeTag TAG_HI_BIT;//={{0x0028,0x0102}};
        const static TypeTag TAG_PX_REP;//={{0x0028,0x0103}};
        const static TypeTag TAG_RESCALE_INT;//={{0x0028,0x1052}};
        const static TypeTag TAG_RESCALE_SLP;//={{0x0028,0x1053}};
        const static TypeTag TAG_FRAME_DATA;//={{0x7fe0,0x0010}};


        ///
        /// default constructor
        ///
        Dicom()
            :_cols(0),
             _rows(0),
             _bits(0),
             _chs(0)
        {
            // nop
        };

        ///
        /// copy constructor
        ///
        /// @param d 
        ///
        Dicom(const Dicom &d,bool compact=false)
        {
            this->_image=d._image.clone();
            this->_cols=d._cols;
            this->_rows=d._rows;
            this->_bits=d._bits;
            this->_chs=d._chs;
            this->_is_signed=d._is_signed;
            
            this->_px_spacing_row=d._px_spacing_row;
            this->_px_spacing_col=d._px_spacing_col;
            this->_image_pos_x=d._image_pos_x;
            this->_image_pos_y=d._image_pos_y;
            this->_image_pos_z=d._image_pos_z;

            this->_architecture_as_little_endian=d._architecture_as_little_endian;
            this->_format_as_little_endian=d._format_as_little_endian;
            this->_format_as_explicit=d._format_as_explicit;
            this->_format_as_deflate=d._format_as_deflate;

            if(!compact){
                this->_element=std::map<uint32_t,Element>(d._element);

                std::map<uint32_t,Element>::iterator itr=
                    this->_element.begin();
                std::map<uint32_t,Element>::iterator itrEnd=
                    this->_element.end();
                for(;itr!=itrEnd;itr++)
                    itr->second._set_parent(this);
            }
        };

        ///
        /// constructor with parse
        ///
        /// @param ist input stream
        ///
        Dicom(std::istream &ist,bool parse_all=true)
        {
            this->parse(ist,parse_all);
        };
        
        ///
        /// destructor
        ///
        ~Dicom()
        {
            this->_element.clear();
        }

        ///
        /// a reader accessor
        ///
        /// @return parsed DICOM image as cv::mat (8bit/16bit 1ch)
        ///
        cv::Mat &image(bool need_rescale=true){
            if(this->_image.empty())
                this->parse_image(need_rescale);

            return this->_image;
        }

        ///
        ///  a reader accessor
        ///
        /// @return image rows or 0
        ///
        int rows(){ return this->_rows; }

        ///
        ///  a reader accessor
        ///
        /// @return image cols or 0
        ///
        int cols(){ return this->_cols; }

        ///
        ///  a reader accessor
        ///
        /// @return bit par pixel or 0
        ///
        int bit_par_pixel(){ return this->_bits; }

        ///
        ///  a reader accessor
        ///
        /// @return image channels or 0
        ///
        int channels(){ return this->_chs; }

        ///
        ///  a reader accessor
        ///
        /// @return true (pixel format is signed) or false
        ///
        bool is_signed(){ return this->_is_signed; }

        ///
        /// a reader accessor
        ///
        /// @return row dir. pixel spacing or 0.0
        ///
        float px_spacing_row(){ return this->_px_spacing_row; }

        ///
        /// a reader accessor
        ///
        /// @return column dir. pixel spacing or 0.0
        ///
        float px_spacing_col(){ return this->_px_spacing_col; }

        ///
        /// a reader accessor
        ///
        /// @return image x position or NaN
        ///
        float image_pos_x(){ return this->_image_pos_x; }

        ///
        /// a reader accessor
        ///
        /// @return image y position or NaN
        ///
        float image_pos_y(){ return this->_image_pos_y; }

        ///
        /// a reader accessor
        ///
        /// @return image z position or NaN
        ///
        float image_pos_z(){ return this->_image_pos_z; }

        
        ///
        /// query method that specify element exists or not
        ///
        /// @param group DICOM element tag group ID
        /// @param id   DICOM element tag ID
        ///
        /// @return true or false
        ///
        bool has_element(const uint16_t group,const uint16_t id)
        {
            TypeTag tag={{group,id}};

            return this->has_element(tag.number);
        }
        bool has_element(const TypeTag tag)
        {
            return this->has_element(tag.number);
        }
        bool has_element(const uint32_t number)
        {
            if(this->_element.find(number)==this->_element.end())
                return false;
            else
                return true;
        }
        
        ///
        /// reader accessor for specify element
        ///
        /// @param group DICOM element tag group ID
        /// @param id   DICOM element tag ID
        ///
        /// @return Element object which has specified tag
        ///
        Element &element(const uint16_t group,const uint16_t id)
        {
            TypeTag tag={{group,id}};

            return this->_element[tag.number];
        }
        Element &element(const TypeTag tag)
        {
            return this->_element[tag.number];
        }
        Element &element(const uint32_t number)
        {
            return this->_element[number];
        }
        Element &operator[](const TypeTag tag)
        {
            return this->_element[tag.number];
        }

        ///
        /// parse DICOM stream
        ///
        /// @param ist input stream
        /// @param parse_all parse with image or only summary
        /// @param need_rescale rescale or not when image parsing
        ///
        /// @return self
        ///
        Dicom &parse(std::istream &ist,
                     bool parse_all=true,
                     bool need_rescale=true)
        {
            if(!ist)
                throw StreamError("Bad stream gaven");

            // test this machine's endian
            uint16_t endian_test=1;
            if(*(char*)&endian_test)
                this->_architecture_as_little_endian=true;
            else
                this->_architecture_as_little_endian=false;

            this->_cols=0;
            this->_rows=0;
            this->_bits=0;
            this->_chs=0;
            this->_is_signed=false;

            this->_px_spacing_row=0.0f;
            this->_px_spacing_col=0.0f;
            this->_image_pos_x=nanf("");
            this->_image_pos_y=nanf("");
            this->_image_pos_z=nanf("");


            this->_element.clear();

            //ist.seekg(0); // rewind stream
            ist.seekg(128); // skip null header

            //
            // check DICOM header
            //
            char dicom_id_str[4];
            ist.read(dicom_id_str,4);

            if(ist.eof() ||
               !ist.good() ||
               dicom_id_str[0]!='D' ||
               dicom_id_str[1]!='I' ||
               dicom_id_str[2]!='C' ||
               dicom_id_str[3]!='M')
                throw ParseError("not DICOM format");

            //
            // read meta data (group 0x0002)
            // as LEE (Little Endian Explicit VR)
            //
            this->_format_as_little_endian=true;
            this->_format_as_explicit=true;
            this->_format_as_deflate=false;

            while(true){
                Element e(this);
                TypeTag tag=e.parse_tag(ist);
                if(tag.id[0]!=TAG_GROUP_META){
                    e.rewind_tag(ist);
                    break;
                }
                e.parse_value(ist);
                this->_element[e.tag().number]=e;
            }


            //
            // get format info.
            //
            if(this->has_element(TAG_TRANSFER_SYNTAX_UID)){
                std::string s=
                    this->element(TAG_TRANSFER_SYNTAX_UID).as<std::string>();
                if(s.find("1.2.840.10008.1.2.2")!=std::string::npos){
                    // BEE
                    this->_format_as_little_endian=false;
                    this->_format_as_explicit=true;
                    this->_format_as_deflate=false;
                }
                else if(s.find("1.2.840.10008.1.2.1.99")!=std::string::npos){
                    // Deflated LEE
                    this->_format_as_little_endian=true;
                    this->_format_as_explicit=true;
                    this->_format_as_deflate=true;
                }
                else if(s.find("1.2.840.10008.1.2.1")!=std::string::npos){
                    // LEE
                    this->_format_as_little_endian=true;
                    this->_format_as_explicit=true;
                    this->_format_as_deflate=false;
                }
                else if(s.find("1.2.840.10008.1.2")!=std::string::npos){
                    // LEI
                    this->_format_as_little_endian=true;
                    this->_format_as_explicit=false;
                    this->_format_as_deflate=false;
                }
            }
            if(this->_format_as_deflate)
                throw std::runtime_error("Deflated LEE has not been supported");

            while(true){
                try{
                    Element e(this,ist);
                    this->_element[e.tag().number]=e;
                }
                catch(StreamError &e){
                    break;
                }
            }

            if(parse_all)
                this->parse_image(need_rescale);
            else
                this->parse_summary();

            return *this;
        }

        ///
        /// parse DICOM tags
        ///
        /// @return self
        ///
        Dicom &parse_summary()
        {
            this->_cols=0;
            this->_rows=0;
            this->_bits=0;
            this->_chs=0;
            this->_is_signed=false;

            this->_px_spacing_row=0.0f;
            this->_px_spacing_col=0.0f;
            this->_image_pos_x=nanf("");
            this->_image_pos_y=nanf("");
            this->_image_pos_z=nanf("");

            if(!this->has_element(TAG_PHOTO_INTERPRET))
                throw MissingTagError(
                    "Could not found Photometric Interpretation Tag");
            std::string p_int=
                this->element(TAG_PHOTO_INTERPRET).as<std::string>();
#ifdef DEBUG
            fprintf(stderr,"\nPhotometric Interpretation: %s\n",p_int.c_str());
#endif

            if(p_int.find("MONOCHROME2")==std::string::npos)
                throw std::runtime_error("Unsupported Photometric Interpretation");

            this->_chs=1;


            //
            // signed or unsigned
            //
            if(!this->has_element(TAG_PX_REP))
                throw MissingTagError(
                    "Could not found Pixel Representation Tag");
            int px_rep=(int)this->element(TAG_PX_REP).as<uint16_t>();
#ifdef DEBUG
            fprintf(stderr,"Pixel Representation: %d\n",px_rep);
#endif
            if(px_rep==0)
                this->_is_signed=false;
            else
                this->_is_signed=true;


            if(!this->has_element(TAG_BIT_ALLOC))
                throw MissingTagError(
                    "Could not found Bit Allocation Tag");
            this->_bits=(int)this->element(TAG_BIT_ALLOC).as<uint16_t>();
#ifdef DEBUG
            fprintf(stderr,"Bit Allocation: %d\n",this->_bits);
#endif

            if(!this->has_element(TAG_ROWS) || 
                !this->has_element(TAG_COLS))
                throw MissingTagError(
                    "Could not found Cols and/or Rows Tag");
            this->_rows=(int)this->element(TAG_ROWS).as<uint16_t>();
            this->_cols=(int)this->element(TAG_COLS).as<uint16_t>();
#ifdef DEBUG
            fprintf(stderr,"%d x %d\n",this->_cols,this->_rows);
#endif


            //
            // misc information
            //
            std::string str;
            std::vector<std::string> s_vec;
            //
            // pixel spacing
            //
            if(this->has_element(TAG_PX_SPACING)){
                str=this->element(TAG_PX_SPACING).as<std::string>();
                boost::algorithm::trim(str);
                boost::algorithm::split(s_vec,str,boost::is_any_of("\\"));
                if(s_vec.size()>=2){
                    try{
                        this->_px_spacing_row=
                            boost::lexical_cast<float>(s_vec[0]);
                        this->_px_spacing_col=
                            boost::lexical_cast<float>(s_vec[1]);

#ifdef DEBUG
                        fprintf(stderr,"Pixel Spacing: %f, %f\n",
                                this->_px_spacing_row,
                                this->_px_spacing_col);
#endif
                    }
                    catch(boost::bad_lexical_cast &e){}
       
                }
            }

            //
            // image position
            //
            if(this->has_element(TAG_IMG_POSITION)){
                s_vec.clear();
                str=this->element(TAG_IMG_POSITION).as<std::string>();
                boost::algorithm::trim(str);
                boost::algorithm::split(s_vec,str,boost::is_any_of("\\"));
                if(s_vec.size()>=3){
                    try{
                        this->_image_pos_x=
                            boost::lexical_cast<float>(s_vec[0]);
                        this->_image_pos_y=
                            boost::lexical_cast<float>(s_vec[1]);
                        this->_image_pos_z=
                            boost::lexical_cast<float>(s_vec[2]);
 
#ifdef DEBUG
                        fprintf(stderr,"Image Position: %f, %f, %f\n",
                                this->_image_pos_x,
                                this->_image_pos_y,
                                this->_image_pos_z);
#endif
                    }
                    catch(boost::bad_lexical_cast &e){}
                }
            }

            return *this;
        }

        Dicom &parse_image(bool need_rescale=true)
        {
            if(!this->_cols ||
               !this->_rows ||
               !this->_bits ||
               !this->_chs)
            this->parse_summary();

            //
            // convert Frame Data (0x7fe0,0x0010) to cv::Mat
            //
            if(!this->has_element(TAG_FRAME_DATA))
                throw MissingTagError(
                    "Could not found Frame Data Tag");
            switch(this->_bits){
            case 8:
                if(this->_is_signed)
                    this->_image=
                        cv::Mat(this->element(TAG_FRAME_DATA).as<
                                    std::vector<char>
                                    >(),
                                true);
                else
                    this->_image=
                        cv::Mat(this->element(TAG_FRAME_DATA).as<
                                    std::vector<unsigned char>
                                    >(),
                                true);
                break;
            case 16:
                if(this->_is_signed)
                    this->_image=
                        cv::Mat(this->element(TAG_FRAME_DATA).as<
                                    std::vector<int16_t>
                                    >(),
                                true);
                else
                    this->_image=
                        cv::Mat(this->element(TAG_FRAME_DATA).as<
                                    std::vector<uint16_t>
                                    >(),
                                true);
                break;
            default:
                throw std::runtime_error("Unsupported Bit Allocation");
            }

            this->_image=this->_image.reshape(1,this->_rows);


            //
            // unpadding for each pixel
            //
            if(!this->has_element(TAG_BIT_STORED))
                throw MissingTagError(
                    "Could not found Bit Stored Tag");
            int bit_stored=(int)this->element(TAG_BIT_STORED).as<uint16_t>();
#ifdef DEBUG
            fprintf(stderr,"Bit Stored: %d\n",bit_stored);
#endif

            if(!this->has_element(TAG_HI_BIT))
                throw MissingTagError(
                    "Could not found Hi Bit Tag");
            int hi_bit=(int)this->element(TAG_HI_BIT).as<uint16_t>();
#ifdef DEBUG
            fprintf(stderr,"Hi Bit: %d\n",hi_bit);
#endif
            if(this->_bits!=bit_stored){
                int d=hi_bit-bit_stored+1;
                for(int i=0;i<d;i++)
                    this->_image/=2;
            }

            std::string str;
            //
            // rescale
            //
            if(need_rescale &&
               this->has_element(TAG_RESCALE_INT) &&
                this->has_element(TAG_RESCALE_SLP)){
                float rescale_interception=0.0;
                float rescale_slope=1.0;

                str=this->element(TAG_RESCALE_INT).as<std::string>();
                boost::algorithm::trim(str);
                try{
                    rescale_interception=boost::lexical_cast<float>(str);
#ifdef DEBUG
                    fprintf(stderr,"Rescale Interception: %f\n",
                            rescale_interception);
#endif
                }
                catch(boost::bad_lexical_cast &e){
                    rescale_interception=0.0;
                }

                str=this->element(TAG_RESCALE_SLP).as<std::string>();
                boost::algorithm::trim(str);
                try{
                    rescale_slope=boost::lexical_cast<float>(str);
#ifdef DEBUG
                    fprintf(stderr,"Rescale Slope: %f\n",
                            rescale_slope);
#endif
                }
                catch(boost::bad_lexical_cast &e){
                    rescale_slope=1.0;
                }

                this->_image*=rescale_slope;
                this->_image+=rescale_interception;
            }

            
            return *this;
        }
        

    private:
        cv::Mat _image;

        int _cols;
        int _rows;
        int _bits;
        int _chs;
        bool _is_signed;

        float _px_spacing_row;
        float _px_spacing_col;
        float _image_pos_x;
        float _image_pos_y;
        float _image_pos_z;

        std::map<uint32_t,Element> _element;

        bool _architecture_as_little_endian;
        bool _format_as_little_endian;
        bool _format_as_explicit;
        bool _format_as_deflate;

        inline bool _need_byte_swap()
        { 
            return this->_architecture_as_little_endian!=
                this->_format_as_little_endian;
        }


    };
};

////////////////////////////////////////////////////////////////////////
//
// class constant definitions
//

//
// Element group tags
//
const uint16_t
VVV::Dicom::TAG_GROUP_META=0x0002,
    VVV::Dicom::TAG_GROUP_DIRECTORY=0x0004;

//
// Element tags
//
const VVV::Dicom::TypeTag
VVV::Dicom::TAG_TRANSFER_SYNTAX_UID={{0x0002,0x0010}},
    VVV::Dicom::TAG_IMG_POSITION={{0x0020,0x0032}},
    VVV::Dicom::TAG_PHOTO_INTERPRET={{0x0028,0x0004}},
    VVV::Dicom::TAG_ROWS={{0x0028,0x0010}},
    VVV::Dicom::TAG_COLS={{0x0028,0x0011}},
    VVV::Dicom::TAG_PX_SPACING={{0x0028,0x0030}},
    VVV::Dicom::TAG_BIT_ALLOC={{0x0028,0x0100}},
    VVV::Dicom::TAG_BIT_STORED={{0x0028,0x0101}},
    VVV::Dicom::TAG_HI_BIT={{0x0028,0x0102}},
    VVV::Dicom::TAG_PX_REP={{0x0028,0x0103}},
    VVV::Dicom::TAG_RESCALE_INT={{0x0028,0x1052}},
    VVV::Dicom::TAG_RESCALE_SLP={{0x0028,0x1053}},
    VVV::Dicom::TAG_FRAME_DATA={{0x7fe0,0x0010}};
//
//
////////////////////////////////////////////////////////////////////////

#endif
