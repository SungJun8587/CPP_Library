
//***************************************************************************
// = 연산자 오버로딩(정수, 실수, 문자열 등 value 할당)
template <typename T>
inline const CRapidXMLUtil& CRapidXMLUtil::operator=(const T& value)
{
    rapidxml::xml_node<char>* node = _doc.first_node()->last_node();

    if constexpr( std::is_arithmetic<T>::value )
    {
        node->value(_doc.allocate_string(std::to_string(value).c_str()));
    }
    else if constexpr( std::is_same_v<T, _tstring> )
    {
        node->value(_doc.allocate_string(TcharToUtf8(value).c_str()));  
    }
    else if constexpr( std::is_same_v<typename std::decay<T>::type, TCHAR*> || std::is_same_v<typename std::decay<T>::type, const TCHAR*> )
    {
        node->value(_doc.allocate_string(TcharToUtf8(value).c_str()));  
    }

    return *this;
}

//***************************************************************************
// XML 파일 저장
template <typename T>
inline bool CRapidXMLUtil::SaveToFile(const T& obj, const _tstring& filename)
{
    std::string xmlContent = Serialize(obj);
    std::ofstream file(filename);
    if( !file.is_open() ) 
    {
        _tcerr << _T("Failed to open file for writing:") << filename << std::endl;
        return false;
    }
    
    file << xmlContent;
    file.close();

    return true;
}

//***************************************************************************
// 파일에서 XML 읽기
template <typename T>
inline T CRapidXMLUtil::LoadFromFile(const _tstring& filename)
{
    std::ifstream file(filename);
    if( !file.is_open() ) 
    {
        _tcerr << _T("Failed to open file for reading:") << filename << std::endl;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return Deserialize<T>(buffer.str());
}

//***************************************************************************
//
template <typename T>
void CRapidXMLUtil::AddNode(const _tstring& nodeName, const T& obj)
{
    ConvertToXML(nodeName, obj);
}

//***************************************************************************
//
template <typename T>
void CRapidXMLUtil::UpdateNode(const _tstring& nodeName, const T& obj)
{
    // 기존 노드 삭제하고 다시 추가
    RemoveNode(nodeName);
    ConvertToXML(nodeName, obj);
}

//***************************************************************************
// XML 직렬화(인덴트 포함)
template <typename T>
inline std::string CRapidXMLUtil::SerializeWithIndent(const T& obj)
{
    _doc.clear();
    xml_node<>* root = _doc.allocate_node(node_type::node_element, RootName);
    _doc.append_node(root);

    SerializeObject(obj, root);

    std::string xmlContent;
    print(std::back_inserter(xmlContent), _doc, 0);
    return xmlContent;
}

//***************************************************************************
// XML 직렬화
template <typename T>
inline std::string CRapidXMLUtil::Serialize(const T& obj)
{
    _doc.clear();
    xml_node<>* root = _doc.allocate_node(node_type::node_element, RootName);
    _doc.append_node(root);

    return UnicodeToUtf8(ConvertToXML(_T(""), obj));
}

//***************************************************************************
// XML 역직렬화
template <typename T>
inline T CRapidXMLUtil::Deserialize(const std::string& xml)
{
    T obj;
    _doc.clear();
    std::vector<char> buffer(xml.begin(), xml.end());
    buffer.push_back('\0');

    try 
    {
        _doc.parse<0>(&buffer[0]);
    }
    catch( const parse_error& e ) 
    {
        _tcerr << _T("XML parse error : ") << e.what() << std::endl;
        _tcerr << _T("Location of error : ") << e.where<TCHAR>() << std::endl;
    }

    xml_node<>* root = _doc.first_node(RootName);
    if( !root ) 
    {
        _tcerr << _T("Root node not found in XML") << std::endl;
    }

    return ConvertFromXML<T>(_T("")).value();
}

//***************************************************************************
// 사용자 정의 객체 추가(직렬화)
template <typename T>
inline void CRapidXMLUtil::AddObject(const T& obj, xml_node<>* parent, const char* tagName)
{
    xml_node<>* classNode = _doc.allocate_node(node_type::node_element, tagName);
    parent->append_node(classNode);
    obj.ToXML(classNode, _doc);
}

//***************************************************************************
// 사용자 정의 객체 가져오기(역직렬화)
template <typename T>
inline void CRapidXMLUtil::GetObject(T& obj, xml_node<>* node)
{
    if( node ) 
    {
        obj.FromXML(node);
    }
}

//***************************************************************************
// 기본 데이터 벡터 추가(직렬화)
template <typename T>
inline void CRapidXMLUtil::AddVector(const std::vector<T>& container, xml_node<>* parent, const char* tagName)
{
    xml_node<>* containerNode = _doc.allocate_node(node_type::node_element, tagName);
    parent->append_node(containerNode);

    for( const auto& item : container )
    {
        if constexpr( std::is_arithmetic<T>::value )
        {
            AddValue(item, containerNode);
        }
        else if constexpr( std::is_same_v<T, _tstring> )
        {
            AddValue(item, containerNode);
        }
    }
}

//***************************************************************************
// 기본 데이터 벡터 가져오기(역직렬화)
template <typename T>
inline void CRapidXMLUtil::GetVector(std::vector<T>& container, xml_node<>* parent, const char* tagName)
{
    xml_node<>* containerNode = parent->first_node(tagName);
    if( containerNode )
    {
        for( xml_node<>* node = containerNode->first_node(ItemName); node; node = node->next_sibling(ItemName) )
        {
            T item;

            if constexpr( std::is_arithmetic<T>::value )
            {
                GetValue(item, node);
            }
            else if constexpr( std::is_same_v<T, _tstring> )
            {
                GetValue(item, node);
            }

            container.push_back(item);
        }
    }
}

//***************************************************************************
// 사용자 정의 객체 벡터 추가(직렬화)
template <typename T>
inline void CRapidXMLUtil::AddObjectVector(const std::vector<T>& container, xml_node<>* parent, const char* tagName)
{
    xml_node<>* containerNode = _doc.allocate_node(node_type::node_element, tagName);
    parent->append_node(containerNode);

    for( const auto& item : container )
    {
        xml_node<>* classNode = _doc.allocate_node(node_type::node_element, ItemName);
        containerNode->append_node(classNode);
        item.ToXML(classNode, _doc);
    }
}

//***************************************************************************
// 사용자 정의 객체 벡터 가져오기(역직렬화)
template <typename T>
inline void CRapidXMLUtil::GetObjectVector(std::vector<T>& container, xml_node<>* parent, const char* tagName)
{
    xml_node<>* containerNode = parent->first_node(tagName);
    if( containerNode )
    {
        for( xml_node<>* node = containerNode->first_node(ItemName); node; node = node->next_sibling(ItemName) )
        {
            T item;
            item.FromXML(node);
            container.push_back(item);
        }
    }
}

//***************************************************************************
// 기본 데이터 맵 추가(직렬화)
template <typename K, typename V>
inline void CRapidXMLUtil::AddMap(const std::map<K, V>& container, xml_node<>* parent, const char* tagName)
{
    xml_node<>* containerNode = _doc.allocate_node(node_type::node_element, tagName);
    parent->append_node(containerNode);

    for( const auto& item : container )
    {
        xml_node<>* itemNode = _doc.allocate_node(node_type::node_element, ItemName);
        containerNode->append_node(itemNode);

        if constexpr( std::is_arithmetic<K>::value )
        {
            AddValue(item.first, itemNode, MapKey);       // 숫자 타입 키 직렬화
        }
        else if constexpr( std::is_same_v<K, _tstring> )
        {
            AddValue(item.first, itemNode, MapKey);       // 문자열 타입 키 직렬화
        }

        if constexpr( std::is_arithmetic<V>::value )
        {
            AddValue(item.second, itemNode, MapValue);    // 숫자 타입 값 직렬화
        }
        else if constexpr( std::is_same_v<V, _tstring> )
        {
            AddValue(item.second, itemNode, MapValue);    // 문자열 타입 값 직렬화
        }
    }
}

//***************************************************************************
// 기본 데이터 맵 가져오기(역직렬화)
template <typename K, typename V>
inline void CRapidXMLUtil::GetMap(std::map<K, V>& container, xml_node<>* parent, const char* tagName)
{
    xml_node<>* containerNode = parent->first_node(tagName);
    if( containerNode ) 
    {
        for( xml_node<>* node = containerNode->first_node(ItemName); node; node = node->next_sibling(ItemName) )
        {
            K key;
            V value;

            xml_node<>* keyNode = node->first_node(MapKey);
            if( keyNode )
            {
                if constexpr( std::is_arithmetic<K>::value )
                {
                    GetValue(key, keyNode);    // 숫자 타입 키 역직렬화
                }
                else if constexpr( std::is_same_v<K, _tstring> )
                {
                    GetValue(key, keyNode);    // 문자열 타입 키 역직렬화
                }
            }

            xml_node<>* valueNode = node->first_node(MapValue);
            if( valueNode )
            {
                if constexpr( std::is_arithmetic<V>::value )
                {
                    GetValue(value, valueNode);    // 숫자 타입 값 역직렬화
                }
                else if constexpr( std::is_same_v<V, _tstring> )
                {
                    GetValue(value, valueNode);    // 문자열 타입 값 역직렬화
                }
            }

            container[key] = value;
        }
    }
}

//***************************************************************************
// 사용자 정의 객체 맵 추가(직렬화)
template <typename K, typename V>
inline void CRapidXMLUtil::AddObjectMap(const std::map<K, V>& container, xml_node<>* parent, const char* tagName)
{
    xml_node<>* containerNode = _doc.allocate_node(node_type::node_element, tagName);
    parent->append_node(containerNode);

    for( const auto& item : container ) 
    {
        xml_node<>* itemNode = _doc.allocate_node(node_type::node_element, ItemName);
        containerNode->append_node(itemNode);

        xml_node<>* node = _doc.allocate_node(node_type::node_element, MapKey, _doc.allocate_string(TcharToUtf8(item.first).c_str()));
        itemNode->append_node(node);

        xml_node<>* classNode = _doc.allocate_node(node_type::node_element, MapValue);
        itemNode->append_node(classNode);
        item.second.ToXML(classNode, _doc);
    }
}

//***************************************************************************
// 사용자 정의 객체 맵 가져오기(역직렬화)
template <typename K, typename V>
inline void CRapidXMLUtil::GetObjectMap(std::map<K, V>& container, xml_node<>* parent, const char* tagName)
{
    xml_node<>* containerNode = parent->first_node(tagName);
    if( containerNode ) 
    {
        for( xml_node<>* node = containerNode->first_node(ItemName); node; node = node->next_sibling(ItemName) )
        {
            K key;
            V value;

            xml_node<>* keyNode = node->first_node(MapKey);
            if( keyNode ) 
            {
                key = Utf8ToTchar(keyNode->value());
            }

            xml_node<>* valueNode = node->first_node(MapValue);
            if( valueNode ) 
            {
                value.FromXML(valueNode);
            }

            container[key] = value;
        }
    }
}

//***************************************************************************
// 문자열 처리
inline void CRapidXMLUtil::AddValue(const _tstring& str, xml_node<>* parent, const char* tagName)
{
    xml_node<>* node = _doc.allocate_node(node_type::node_element, tagName, _doc.allocate_string(TcharToUtf8(str).c_str()));
    parent->append_node(node);
}

//***************************************************************************
//
inline void CRapidXMLUtil::GetValue(_tstring& str, xml_node<>* node)
{
    if( node ) str = Utf8ToTchar(node->value());
}

//***************************************************************************
// 숫자 타입 처리
template <typename T, typename std::enable_if<std::is_arithmetic<T>::value>::type*>
inline void CRapidXMLUtil::AddValue(const T& value, xml_node<>* parent, const char* tagName)
{
    xml_node<>* node = _doc.allocate_node(node_type::node_element, tagName, _doc.allocate_string(std::to_string(value).c_str()));
    parent->append_node(node);
}

//***************************************************************************
//
template <typename T, typename std::enable_if<std::is_arithmetic<T>::value>::type*>
inline void CRapidXMLUtil::GetValue(T& value, xml_node<>* node)
{
    if( node ) value = static_cast<T>(std::stod(node->value()));
}

//***************************************************************************
// 타입별 XML 값 변환
template <typename T>
inline _tstring CRapidXMLUtil::ConvertToXML(const _tstring& nodeName, const T& obj)
{
    xml_node<char>* root = _doc.first_node();
    if( !root )
    {
        root = _doc.allocate_node(node_type::node_element, RootName);
        _doc.append_node(root);
    }

    char* tagName = nullptr;

    if( nodeName.size() > 0 )
        tagName = _doc.allocate_string(TcharToUtf8(nodeName).c_str());

    if constexpr( is_vector<T>::value )
    {
        using ValueType = typename T::value_type;
        if constexpr( std::is_arithmetic_v<ValueType> || std::is_same_v<ValueType, _tstring> )
        {
            AddVector(obj, root, tagName != nullptr ? tagName : VectorName);        // 기본 자료형 벡터
        }
        else
        {
            AddObjectVector(obj, root, tagName != nullptr ? tagName : VectorName);  // 사용자 정의 클래스 벡터
        }
    }
    else if constexpr( is_map<T>::value )
    {
        // 맵 처리
        using KeyType = typename T::key_type;
        using ValueType = typename T::mapped_type;
        if constexpr( std::is_same_v<KeyType, _tstring> )
        {
            // 키가 _tstring인 경우만 처리
            if constexpr( std::is_arithmetic_v<ValueType> || std::is_same_v<ValueType, _tstring> )
            {
                AddMap(obj, root, tagName != nullptr ? tagName : MapName);           // 기본 자료형 맵
            }
            else
            {
                AddObjectMap(obj, root, tagName != nullptr ? tagName : MapName);     // 사용자 정의 클래스 맵
            }
        }
    }
    else if constexpr( has_toxml_method<T>::value )
    {
        AddObject(obj, root, tagName != nullptr ? tagName : ItemName);
    }
    else if constexpr( std::is_arithmetic<T>::value )
    {
        AddValue(obj, root, tagName != nullptr ? tagName : ItemName);
    }
    else if constexpr( std::is_same_v<T, _tstring> )
    {
        AddValue(obj, root, tagName != nullptr ? tagName : ItemName);
    }
    else if constexpr( std::is_same_v<typename std::decay<T>::type, TCHAR*> || std::is_same_v<typename std::decay<T>::type, const TCHAR*> )
    {
        AddValue(obj, root, tagName != nullptr ? tagName : ItemName);
    }

    std::ostringstream oss;
    oss << _doc;
    return Utf8ToUnicode(oss.str());
}

//***************************************************************************
// XML 노드 값을 반환
template <typename T>
inline std::optional<T> CRapidXMLUtil::ConvertFromXML(const _tstring& nodeName)
{
    T value;

    char* tagName = nullptr;

    if( nodeName.size() > 0 )
        tagName = _doc.allocate_string(TcharToUtf8(nodeName).c_str());

    xml_node<char>* root = _doc.first_node();
    if( !root ) return std::nullopt;        // 노드가 없거나 값이 없으면 std::nullopt 반환

    if constexpr( is_vector<T>::value )
    {
        using ValueType = typename T::value_type;
        if constexpr( std::is_arithmetic_v<ValueType> || std::is_same_v<ValueType, _tstring> )
        {
            GetVector<ValueType>(value, root, tagName != nullptr ? tagName : VectorName);        // 기본 자료형 벡터
        }
        else
        {
            GetObjectVector<ValueType>(value, root, tagName != nullptr ? tagName : VectorName);  // 사용자 정의 클래스 벡터
        }
    }
    else if constexpr( is_map<T>::value )
    {
        // 맵 처리
        using KeyType = typename T::key_type;
        using ValueType = typename T::mapped_type;
        if constexpr( std::is_same_v<KeyType, _tstring> )
        {
            // 키가 _tstring인 경우만 처리
            if constexpr( std::is_arithmetic_v<ValueType> || std::is_same_v<ValueType, _tstring> )
            {
                GetMap<KeyType, ValueType>(value, root, tagName != nullptr ? tagName : MapName);           // 기본 자료형 맵
            }
            else
            {
                GetObjectMap<KeyType, ValueType>(value, root, tagName != nullptr ? tagName : MapName);     // 사용자 정의 클래스 맵
            }
        }
    }
    else if constexpr( has_toxml_method<T>::value )
    {
        GetObject<T>(value, root->first_node(tagName));
    }
    else if constexpr( std::is_same_v<T, _tstring> )
    {
        GetValue(value, root->first_node(tagName));
    }
    else if constexpr( std::is_arithmetic<T>::value )
    {
        GetValue(value, root->first_node(tagName));
    }

    return optional<T>(value);
}