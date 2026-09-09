import { cva, type VariantProps } from "class-variance-authority";
import { cn } from "@/lib/utils";
import type { ButtonHTMLAttributes } from "react";

const buttonVariants = cva(
  "inline-flex items-center justify-center gap-2 font-medium tracking-wide uppercase transition-opacity duration-150 ease-out disabled:pointer-events-none disabled:opacity-40 focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-accent/70",
  {
    variants: {
      variant: {
        primary: "bg-accent text-accent-fg hover:opacity-90 active:scale-[0.98]",
        ghost: "bg-transparent text-fg border border-border hover:bg-surface-2",
        danger: "bg-danger text-fg hover:opacity-90",
        quiet: "bg-surface-2 text-fg border border-border hover:border-accent/40",
      },
      size: {
        md: "h-11 px-5 text-sm rounded-md",
        sm: "h-9 px-3 text-xs rounded-sm",
        lg: "h-12 px-6 text-base rounded-lg",
        icon: "size-11 rounded-md",
      },
    },
    defaultVariants: { variant: "primary", size: "md" },
  },
);

type Props = ButtonHTMLAttributes<HTMLButtonElement> & VariantProps<typeof buttonVariants>;

export function Button({ className, variant, size, ...props }: Props) {
  return <button className={cn(buttonVariants({ variant, size }), className)} {...props} />;
}
