//
// This file was generated by the BinaryNotes compiler.
// See http://bnotes.sourceforge.net 
// Any modifications to this file will be lost upon recompilation of the source ASN.1. 
//

using GSF.ASN1;
using GSF.ASN1.Attributes;
using GSF.ASN1.Coders;
using GSF.ASN1.Types;

namespace GSF.MMS.Model
{
    
    [ASN1PreparedElement]
    [ASN1Choice(Name = "VariableSpecification")]
    public class VariableSpecification : IASN1PreparedElement
    {
        private static readonly IASN1PreparedElementData preparedData = CoderFactory.getInstance().newPreparedElementData(typeof(VariableSpecification));
        private Address address_;
        private bool address_selected;
        private NullObject invalidated_;
        private bool invalidated_selected;
        private ObjectName name_;
        private bool name_selected;
        private ScatteredAccessDescription scatteredAccessDescription_;
        private bool scatteredAccessDescription_selected;


        private VariableDescriptionSequenceType variableDescription_;
        private bool variableDescription_selected;

        [ASN1Element(Name = "name", IsOptional = false, HasTag = true, Tag = 0, HasDefaultValue = false)]
        public ObjectName Name
        {
            get
            {
                return name_;
            }
            set
            {
                selectName(value);
            }
        }

        [ASN1Element(Name = "address", IsOptional = false, HasTag = true, Tag = 1, HasDefaultValue = false)]
        public Address Address
        {
            get
            {
                return address_;
            }
            set
            {
                selectAddress(value);
            }
        }


        [ASN1Element(Name = "variableDescription", IsOptional = false, HasTag = true, Tag = 2, HasDefaultValue = false)]
        public VariableDescriptionSequenceType VariableDescription
        {
            get
            {
                return variableDescription_;
            }
            set
            {
                selectVariableDescription(value);
            }
        }


        [ASN1Element(Name = "scatteredAccessDescription", IsOptional = false, HasTag = true, Tag = 3, HasDefaultValue = false)]
        public ScatteredAccessDescription ScatteredAccessDescription
        {
            get
            {
                return scatteredAccessDescription_;
            }
            set
            {
                selectScatteredAccessDescription(value);
            }
        }


        [ASN1Null(Name = "invalidated")]
        [ASN1Element(Name = "invalidated", IsOptional = false, HasTag = true, Tag = 4, HasDefaultValue = false)]
        public NullObject Invalidated
        {
            get
            {
                return invalidated_;
            }
            set
            {
                selectInvalidated(value);
            }
        }

        public void initWithDefaults()
        {
        }

        public IASN1PreparedElementData PreparedData
        {
            get
            {
                return preparedData;
            }
        }


        public bool isNameSelected()
        {
            return name_selected;
        }


        public void selectName(ObjectName val)
        {
            name_ = val;
            name_selected = true;


            address_selected = false;

            variableDescription_selected = false;

            scatteredAccessDescription_selected = false;

            invalidated_selected = false;
        }


        public bool isAddressSelected()
        {
            return address_selected;
        }


        public void selectAddress(Address val)
        {
            address_ = val;
            address_selected = true;


            name_selected = false;

            variableDescription_selected = false;

            scatteredAccessDescription_selected = false;

            invalidated_selected = false;
        }


        public bool isVariableDescriptionSelected()
        {
            return variableDescription_selected;
        }


        public void selectVariableDescription(VariableDescriptionSequenceType val)
        {
            variableDescription_ = val;
            variableDescription_selected = true;


            name_selected = false;

            address_selected = false;

            scatteredAccessDescription_selected = false;

            invalidated_selected = false;
        }


        public bool isScatteredAccessDescriptionSelected()
        {
            return scatteredAccessDescription_selected;
        }


        public void selectScatteredAccessDescription(ScatteredAccessDescription val)
        {
            scatteredAccessDescription_ = val;
            scatteredAccessDescription_selected = true;


            name_selected = false;

            address_selected = false;

            variableDescription_selected = false;

            invalidated_selected = false;
        }


        public bool isInvalidatedSelected()
        {
            return invalidated_selected;
        }


        public void selectInvalidated()
        {
            selectInvalidated(new NullObject());
        }


        public void selectInvalidated(NullObject val)
        {
            invalidated_ = val;
            invalidated_selected = true;


            name_selected = false;

            address_selected = false;

            variableDescription_selected = false;

            scatteredAccessDescription_selected = false;
        }

        [ASN1PreparedElement]
        [ASN1Sequence(Name = "variableDescription", IsSet = false)]
        public class VariableDescriptionSequenceType : IASN1PreparedElement
        {
            private static IASN1PreparedElementData preparedData = CoderFactory.getInstance().newPreparedElementData(typeof(VariableDescriptionSequenceType));
            private Address address_;


            private TypeSpecification typeSpecification_;

            [ASN1Element(Name = "address", IsOptional = false, HasTag = false, HasDefaultValue = false)]
            public Address Address
            {
                get
                {
                    return address_;
                }
                set
                {
                    address_ = value;
                }
            }

            [ASN1Element(Name = "typeSpecification", IsOptional = false, HasTag = false, HasDefaultValue = false)]
            public TypeSpecification TypeSpecification
            {
                get
                {
                    return typeSpecification_;
                }
                set
                {
                    typeSpecification_ = value;
                }
            }


            public void initWithDefaults()
            {
            }

            public IASN1PreparedElementData PreparedData
            {
                get
                {
                    return preparedData;
                }
            }
        }
    }
}